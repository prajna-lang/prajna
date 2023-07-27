
#include "prajna/jit/execution_engine.h"

#ifdef PRAJNA_WITH_GPU
#include <cuda.h>
#include <cuda_runtime.h>
#include <nvrtc.h>
#endif

#include <setjmp.h>
#include <stdio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "boost/dll/shared_library.hpp"
#include "llvm/ExecutionEngine/JITLink/JITLinkMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/EPCDynamicLibrarySearchGenerator.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/DynamicLibrary.h"
#include "prajna/assert.hpp"
#include "prajna/compiler/compiler.h"
#include "prajna/exception.hpp"
#include "prajna/ir/ir.hpp"

#ifdef __linux__
extern "C" uint16_t __truncdfhf2(double);
extern "C" uint16_t __floattihf(int64_t[2]);
extern "C" uint16_t __floatuntihf(uint64_t[2]);
#endif

namespace prajna {

std::unordered_map<void *, std::atomic<int64_t>> ptr_count_dict;

}  // namespace prajna

namespace prajna::jit {

#ifdef PRAJNA_WITH_GPU
// cudaMalloc是一个重载函数, 故重新包装了一下
cudaError_t cudaMalloc(void **devPtr, size_t size) { return ::cudaMalloc(devPtr, size); }

inline void checkCudaErrors(bool re) { PRAJNA_ASSERT(re == 0); }
#endif

jmp_buf buf;

void print_c(const char *c_str) { print_callback(std::string(c_str)); }
char *input_c() { return input_callback(); }
void exit_c(int64_t ret_code) {
    std::string msg = "exit " + std::to_string(ret_code) + "\n";
    print_c(msg.c_str());
    longjmp(buf, 1);
}

float Clock() {
    // steady_clock是统计物理世界的时间
    return std::chrono::steady_clock::now().time_since_epoch().count() * 1.0 *
           std::chrono::steady_clock::period::num / std::chrono::steady_clock::period::den;
}

void Sleep(float t) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<long long>(t * 1000000000)));
}

llvm::sys::DynamicLibrary __load_dynamic_library(char *lib_name) {
    auto dl = llvm::sys::DynamicLibrary::getLibrary(lib_name);
    return dl;
}

void *__get_symbol(llvm::sys::DynamicLibrary dl, char *symbol_name) {
    return dl.getAddressOfSymbol(symbol_name);
}

void __close_dynamic_library(llvm::sys::DynamicLibrary dl) {
    llvm::sys::DynamicLibrary::closeLibrary(dl);
}

llvm::ExitOnError exit_on_error;

ExecutionEngine::ExecutionEngine() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    // LLVMInitializeNativeAsmParser();

// TODO: 需要确定setObjectLinkingLayerCreator的作用, 现在去除后, 在mac上会报错.
#ifdef __APPLE__
    auto expect_up_lljit =
        llvm::orc::LLJITBuilder()
            .setObjectLinkingLayerCreator(
                [=](llvm::orc::ExecutionSession &ES, const llvm::Triple &TT) {
                    // @note 需要确认机制是做什么用的
                    auto ll = std::make_unique<llvm::orc::ObjectLinkingLayer>(
                        ES, std::make_unique<llvm::jitlink::InProcessMemoryManager>(64 * 1024));
                    ll->setAutoClaimResponsibilityForObjectSymbols(true);
                    return std::move(ll);
                })
            .create();
#else
    auto expect_up_lljit = llvm::orc::LLJITBuilder().create();
#endif
    PRAJNA_ASSERT(expect_up_lljit);
    _up_lljit = std::move(*expect_up_lljit);

    _up_lljit->getMainJITDylib().addGenerator(
        cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            _up_lljit->getDataLayout().getGlobalPrefix())));
}

bool ExecutionEngine::LoadDynamicLib(std::string lib_name) {
    return llvm::sys::DynamicLibrary::getPermanentLibrary(lib_name.c_str()).isValid();
}

size_t ExecutionEngine::GetValue(std::string name) {
    auto expect_symbol = _up_lljit->lookup(name);
    PRAJNA_VERIFY(expect_symbol);
    return expect_symbol->getValue();
}

void ExecutionEngine::AddIRModule(std::shared_ptr<ir::Module> ir_module) {
    // host
    auto up_llvm_module = std::unique_ptr<llvm::Module>(ir_module->llvm_module);
    llvm::orc::ThreadSafeModule llvm_orc_thread_module(std::move(up_llvm_module),
                                                       std::make_unique<llvm::LLVMContext>());
    exit_on_error(_up_lljit->addIRModule(std::move(llvm_orc_thread_module)));

    // auto =  ir_module->modules[ir::Tar]

#ifdef PRAJNA_WITH_GPU
    for (auto [ir_target, ir_sub_module] : ir_module->modules) {
        if (not ir_sub_module) continue;

        if (std::none_of(RANGE(ir_sub_module->functions),
                         [](std::shared_ptr<ir::Function> ir_function) {
                             return ir_function->annotation_dict.count("kernel");
                         })) {
            continue;
        }

        // 目前仅支持nvptx后端的gpu
        PRAJNA_ASSERT(ir_target == ir::Target::nvptx);
        auto file_base = (std::filesystem::temp_directory_path() /
                          std::filesystem::path(ir_sub_module->name).stem())
                             .string();
        std::error_code err_code;
        llvm::raw_fd_ostream llvm_fs(file_base + ".ll", err_code);
        ir_sub_module->llvm_module->print(llvm_fs, nullptr);
        llvm_fs.close();
        PRAJNA_VERIFY(
            std::system(("llc " + file_base + ".ll" + " -o " + file_base + ".ptx").c_str()) == 0);

        auto cu_re = cuInit(0);
        PRAJNA_ASSERT(cu_re == CUDA_SUCCESS);
        CUdevice cu_device;
        cu_re = cuDeviceGet(&cu_device, 0);
        PRAJNA_ASSERT(cu_re == CUDA_SUCCESS);
        auto cuda_re = cudaSetDevice(cu_device);
        PRAJNA_ASSERT(cuda_re == cudaSuccess);
        CUcontext cu_context;
        cu_re = cuCtxCreate(&cu_context, 0, cu_device);
        if (cu_re == CUDA_ERROR_OUT_OF_MEMORY) {
            throw std::runtime_error("cuda error out of memory");
        }
        PRAJNA_ASSERT(cu_re == CUDA_SUCCESS);

        const size_t options_size = 6;
        CUjit_option options[options_size];
        void *optionVals[options_size];
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

        CUmodule cu_module = 0;
        CUfunction cu_kernel_funciton = 0;
        CUlinkState cu_link_state;

        cu_re = cuLinkCreate(options_size, options, optionVals, &cu_link_state);
        PRAJNA_ASSERT(cu_re == CUDA_SUCCESS, std::string(error_log));
        std::fstream fs(file_base + ".ptx");
        std::stringstream ss;
        ss << fs.rdbuf();
        auto ptx_source = ss.str();

        cu_re =
            cuLinkAddData(cu_link_state, CU_JIT_INPUT_PTX, const_cast<char *>(ptx_source.c_str()),
                          strlen(ptx_source.c_str()) + 1, 0, 0, 0, 0);
        PRAJNA_ASSERT(cu_re == CUDA_SUCCESS, std::string(error_log));

        cu_re = cuLinkComplete(cu_link_state, &cuOut, &outSize);
        PRAJNA_ASSERT(cu_re == CUDA_SUCCESS, std::string(error_log));

        // printf("CUDA Link Completed in %fms. Linker Output:\n%s\n", walltime, info_log);
        cu_re = cuModuleLoadData(&cu_module, cuOut);
        PRAJNA_ASSERT(cu_re == CUDA_SUCCESS);

        for (auto ir_function : ir_sub_module->functions) {
            if (ir_function->annotation_dict.count("kernel")) {
                auto kernel_fun_address_name = GetKernelFunctionAddressName(ir_function);
                auto test_kernel_fun =
                    reinterpret_cast<CUfunction *>(this->GetValue(kernel_fun_address_name));
                std::string function_name = MangleNvvmName(ir_function->fullname);
                cu_re = cuModuleGetFunction(test_kernel_fun, cu_module, function_name.c_str());
                PRAJNA_ASSERT(cu_re == CUDA_SUCCESS, std::string(error_log));
            }
        }

        // TODO 一旦释放, 核函数等对象就无效了, 后面需要再进一步优化
        // cuCtxDestroy(cu_context);
    }
#endif
}

void ExecutionEngine::BindCFunction(void *fun_ptr, std::string mangle_name) {
    auto fun_symbol = llvm::orc::absoluteSymbols(
        {{_up_lljit->mangleAndIntern(mangle_name),
          {llvm::orc::ExecutorAddr::fromPtr(fun_ptr),
           llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Absolute}}});
    exit_on_error(_up_lljit->getMainJITDylib().define(fun_symbol));
}

void ExecutionEngine::CatchRuntimeError() {
    auto error = setjmp(buf);
    if (error != 0) {
        throw RuntimeError();
    }
}

void ExecutionEngine::BindBuiltinFunction() {
    this->BindCFunction(reinterpret_cast<void *>(exit_c), "::bindings::exit");
    this->BindCFunction(reinterpret_cast<void *>(malloc), "::bindings::malloc");
    this->BindCFunction(reinterpret_cast<void *>(free), "::bindings::free");
    this->BindCFunction(reinterpret_cast<void *>(getchar), "::bindings::getchar");

    this->BindCFunction(reinterpret_cast<void *>(print_c), "::bindings::print");
    this->BindCFunction(reinterpret_cast<void *>(input_c), "::bindings::input");

#ifdef __linux__
    this->BindCFunction(reinterpret_cast<void *>(__truncdfhf2), "__truncdfhf2");
    // this->BindCFunction(reinterpret_cast<void *>(__floattihf), "__floattihf");
    // this->BindCFunction(reinterpret_cast<void *>(__floatuntihf), "__floatuntihf");
#endif

    this->BindCFunction(reinterpret_cast<void *>(fopen), "::fs::_c::fopen");
    this->BindCFunction(reinterpret_cast<void *>(fclose), "::fs::_c::fclose");
    this->BindCFunction(reinterpret_cast<void *>(fseek), "::fs::_c::fseek");
    this->BindCFunction(reinterpret_cast<void *>(ftell), "::fs::_c::ftell");
    this->BindCFunction(reinterpret_cast<void *>(fflush), "::fs::_c::fflush");
    this->BindCFunction(reinterpret_cast<void *>(fread), "::fs::_c::fread");
    this->BindCFunction(reinterpret_cast<void *>(fwrite), "::fs::_c::fwrite");

    this->BindCFunction(reinterpret_cast<void *>(Clock), "::chrono::Clock");
    this->BindCFunction(reinterpret_cast<void *>(Sleep), "::chrono::Sleep");

    this->BindCFunction(reinterpret_cast<void *>(__load_dynamic_library),
                        "::__load_dynamic_library");
    this->BindCFunction(reinterpret_cast<void *>(__get_symbol), "::__get_symbol");
    this->BindCFunction(reinterpret_cast<void *>(__close_dynamic_library),
                        "::__close_dynamic_library");

#ifdef PRAJNA_WITH_GPU
    this->BindCFunction(reinterpret_cast<void *>(cuInit), "::cuda::cuInit");
    this->BindCFunction(reinterpret_cast<void *>(cuDeviceGetCount), "::cuda::cuDeviceGetCount");
    this->BindCFunction(reinterpret_cast<void *>(cudaDeviceSynchronize),
                        "::cuda::cudaDeviceSynchronize");
    this->BindCFunction(reinterpret_cast<void *>(cuDeviceGetAttribute),
                        "::cuda::cuDeviceGetAttribute");
    this->BindCFunction(reinterpret_cast<void *>(cuLaunchKernel), "::cuda::cuLaunchKernel");
    this->BindCFunction(reinterpret_cast<void *>(cudaSetDevice), "::cuda::cudaSetDevice");
    this->BindCFunction(reinterpret_cast<void *>(::cudaMemcpy), "::cuda::cudaMemcpy");
    // cudaMalloc是一个重载函数, 故重新包装了一下
    this->BindCFunction(reinterpret_cast<void *>(cudaMalloc), "::cuda::cudaMalloc");
    this->BindCFunction(reinterpret_cast<void *>(::cuMemAlloc), "::cuda::cuMemAlloc");
#endif
}

}  // namespace prajna::jit
