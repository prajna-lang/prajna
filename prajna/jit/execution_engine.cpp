
#include "prajna/jit/execution_engine.h"

#ifdef PRAJNA_WITH_GPU
#include <cuda.h>
#include <cuda_runtime.h>
#include <nvrtc.h>
#endif

#include <setjmp.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>

#include "llvm/ExecutionEngine/JITLink/JITLinkMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "prajna/assert.hpp"
#include "prajna/ir/ir.hpp"

namespace prajna::jit {

inline void checkCudaErrors(bool re) { PRAJNA_ASSERT(re == 0); }

jmp_buf buf;

void c_jmp_exit() { longjmp(buf, 1); }
void c_print(char *char_string) { printf("%s", char_string); }
void c_assert(bool t) {
    if (!t) {
        printf("%s\n", "Prajna runtime error");
        longjmp(buf, 1);
    }
}

std::map<void *, std::atomic<int64_t>> ptr_count_dict;

void registerReferenceCount(void *ptr) { ptr_count_dict[ptr].store(0); }

int64_t getReferenceCount(void *ptr) { return ptr_count_dict[ptr].load(); }

void incReferenceCount(void *ptr) { ++ptr_count_dict[ptr]; }

void decReferenceCount(void *ptr) { --ptr_count_dict[ptr]; }

llvm::ExitOnError exit_on_error;

ExecutionEngine::ExecutionEngine() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    // LLVMInitializeNativeAsmParser();

    auto expect_up_lljit =
        llvm::orc::LLJITBuilder()
            .setObjectLinkingLayerCreator(
                [&](llvm::orc::ExecutionSession &ES, const llvm::Triple &TT) {
                    auto ll = std::make_unique<llvm::orc::ObjectLinkingLayer>(
                        ES, std::make_unique<llvm::jitlink::InProcessMemoryManager>(64 * 1024));
                    ll->setAutoClaimResponsibilityForObjectSymbols(true);
                    return std::move(ll);
                })
            .create();
    PRAJNA_ASSERT(expect_up_lljit);
    _up_lljit = std::move(*expect_up_lljit);
}

size_t ExecutionEngine::getAddress(std::string name) {
    auto expect_symbol = _up_lljit->lookup(name);
    PRAJNA_ASSERT(expect_symbol);
    return expect_symbol->getValue();
}

void ExecutionEngine::addIRModule(std::shared_ptr<ir::Module> ir_module) {
    // host
    auto up_llvm_module = std::unique_ptr<llvm::Module>(ir_module->llvm_module);
    llvm::orc::ThreadSafeModule llvm_orc_thread_module(std::move(up_llvm_module),
                                                       std::make_unique<llvm::LLVMContext>());
    exit_on_error(_up_lljit->addIRModule(std::move(llvm_orc_thread_module)));

    // auto =  ir_module->modules[ir::Tar]

#ifdef PRAJNA_WITH_GPU
    for (auto [ir_target, ir_sub_module] : ir_module->modules) {
        if (ir_sub_module == nullptr) continue;

        // 目前仅支持nvptx后端的gpu
        PRAJNA_ASSERT(ir_target == ir::Target::nvptx);
        auto file_base = (std::filesystem::temp_directory_path() /
                          std::filesystem::path(ir_sub_module->name).stem())
                             .string();
        std::error_code err_code;
        llvm::raw_fd_ostream llvm_fs(file_base + ".ll", err_code);
        ir_sub_module->llvm_module->print(llvm_fs, nullptr);
        llvm_fs.close();
        auto re = std::system(("llc " + file_base + ".ll" + " -o " + file_base + ".ptx").c_str());
        PRAJNA_ASSERT(re == 0);

        cuInit(0);
        CUdevice cuDevice;
        cuDeviceGet(&cuDevice, 0);
        cudaSetDevice(cuDevice);
        CUcontext cuContext;
        cuCtxCreate(&cuContext, 0, cuDevice);

        CUjit_option options[6];
        void *optionVals[6];
        float walltime;
        char error_log[8192], info_log[8192];
        unsigned int logSize = 8192;
        void *cuOut;
        size_t outSize;

        // Setup linker options
        // Return walltime from JIT compilation
        options[0] = CU_JIT_WALL_TIME;
        optionVals[0] = (void *)&walltime;
        // Pass a buffer for info messages
        options[1] = CU_JIT_INFO_LOG_BUFFER;
        optionVals[1] = (void *)info_log;
        // Pass the size of the info buffer
        options[2] = CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES;
        optionVals[2] = (void *)(long)logSize;
        // Pass a buffer for error message
        options[3] = CU_JIT_ERROR_LOG_BUFFER;
        optionVals[3] = (void *)error_log;
        // Pass the size of the error buffer
        options[4] = CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES;
        optionVals[4] = (void *)(long)logSize;
        // Make the linker verbose
        options[5] = CU_JIT_LOG_VERBOSE;
        optionVals[5] = (void *)1;

        CUmodule hModule = 0;
        CUfunction hKernel = 0;
        CUlinkState lState;
        int *d_data = 0;
        int *h_data = 0;

        PRAJNA_ASSERT(cuLinkCreate(6, options, optionVals, &lState) == CUDA_SUCCESS,
                      std::string(error_log));
        std::fstream fs(file_base + ".ptx");
        std::stringstream ss;
        ss << fs.rdbuf();
        auto ptx_source = ss.str();

        PRAJNA_ASSERT(
            cuLinkAddData(lState, CU_JIT_INPUT_PTX, const_cast<char *>(ptx_source.c_str()),
                          strlen(ptx_source.c_str()) + 1, 0, 0, 0, 0) == CUDA_SUCCESS,
            std::string(error_log));

        PRAJNA_ASSERT(cuLinkComplete(lState, &cuOut, &outSize) == CUDA_SUCCESS,
                      std::string(error_log));

        // printf("CUDA Link Completed in %fms. Linker Output:\n%s\n", walltime, info_log);

        PRAJNA_ASSERT(cuModuleLoadData(&hModule, cuOut) == CUDA_SUCCESS);

        for (auto ir_function : ir_sub_module->functions) {
            if (ir_function->function_type->annotations.count("kernel")) {
                auto kernel_fun_address_name = getKernelFunctionAddressName(ir_function->fullname);
                auto test_kernel_fun =
                    reinterpret_cast<CUfunction *>(this->getAddress(kernel_fun_address_name));
                std::string function_name = mangleNvvmName(ir_function->fullname);
                auto re = cuModuleGetFunction(test_kernel_fun, hModule, function_name.c_str());
                PRAJNA_ASSERT(re == CUDA_SUCCESS, std::string(error_log));
            }
        }
    }
#endif
}

void ExecutionEngine::bindCFunction(void *fun_ptr, std::string mangle_name) {
    _up_lljit->getMainJITDylib().addGenerator(
        cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            _up_lljit->getDataLayout().getGlobalPrefix())));
    auto fun_symbol = llvm::orc::absoluteSymbols(
        {{_up_lljit->mangleAndIntern(mangle_name),
          {llvm::pointerToJITTargetAddress(fun_ptr),
           llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Absolute}}});
    exit_on_error(_up_lljit->getMainJITDylib().define(fun_symbol));
}

void ExecutionEngine::catchRuntimeError() {
    auto error = setjmp(buf);
    if (error != 0) {
        throw std::runtime_error("Prajna Runtime Error");
    }
}

void ExecutionEngine::bindBuiltinFunction() {
    this->bindCFunction(reinterpret_cast<void *>(c_jmp_exit), "::bindings::jmp_exit_c");
    this->bindCFunction(reinterpret_cast<void *>(c_print), "::bindings::print_c");
    this->bindCFunction(reinterpret_cast<void *>(c_assert), "::bindings::assert");
    this->bindCFunction(reinterpret_cast<void *>(malloc), "::bindings::malloc");
    this->bindCFunction(reinterpret_cast<void *>(free), "::bindings::free");

    this->bindCFunction(reinterpret_cast<void *>(registerReferenceCount),
                        "::bindings::registerReferenceCount");
    this->bindCFunction(reinterpret_cast<void *>(getReferenceCount),
                        "::bindings::getReferenceCount");
    this->bindCFunction(reinterpret_cast<void *>(incReferenceCount),
                        "::bindings::incReferenceCount");
    this->bindCFunction(reinterpret_cast<void *>(decReferenceCount),
                        "::bindings::decReferenceCount");

#ifdef PRAJNA_WITH_GPU
    this->bindCFunction(reinterpret_cast<void *>(cuInit), "::cuda::cuInit");
    this->bindCFunction(reinterpret_cast<void *>(cuDeviceGetCount), "::cuda::cuDeviceGetCount");
    this->bindCFunction(reinterpret_cast<void *>(cuDeviceGetAttribute),
                        "::cuda::cuDeviceGetAttribute");
    this->bindCFunction(reinterpret_cast<void *>(cuLaunchKernel), "::cuda::cuLaunchKernel");
    this->bindCFunction(reinterpret_cast<void *>(::cudaMemcpy), "::cuda::cudaMemcpy");
    this->bindCFunction(reinterpret_cast<void *>(::cuMemAlloc), "::cuda::cuMemAlloc");
#endif
}

}  // namespace prajna::jit
