
#include "prajna/jit/execution_engine.h"

#include <setjmp.h>
#include <stdio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include "boost/dll/shared_library.hpp"
#include "fmt/format.h"
#include "llvm/ExecutionEngine/JITLink/JITLinkMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/EPCDynamicLibrarySearchGenerator.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ObjectTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/DynamicLibrary.h"
#include "prajna/assert.hpp"
#include "prajna/compiler/compiler.h"
#include "prajna/exception.hpp"
#include "prajna/helper.hpp"
#include "prajna/ir/ir.hpp"
#include "prajna/jit/gpu_compiler.hpp"
#include "prajna/jit/hip_runtime_loader.cpp"

#if defined(__linux__) || defined(WIN32)
extern "C" uint16_t __truncdfhf2(double);
extern "C" uint16_t __floattihf(int64_t[2]);
extern "C" uint16_t __floatuntihf(uint64_t[2]);
#endif

namespace prajna {

std::unordered_map<void *, std::atomic<int64_t>> ptr_count_dict;

}  // namespace prajna

namespace prajna::jit {

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
#ifdef PRAJNA_WITH_CUDA
    // 初始化NVPTX目标
    LLVMInitializeNVPTXTarget();
    LLVMInitializeNVPTXTargetInfo();
    LLVMInitializeNVPTXTargetMC();
    LLVMInitializeNVPTXAsmPrinter();
#endif
#ifdef PRAJNA_WITH_ROCM
    LLVMInitializeAMDGPUTarget();
    LLVMInitializeAMDGPUTargetInfo();
    LLVMInitializeAMDGPUTargetMC();
    LLVMInitializeAMDGPUAsmPrinter();
#endif

    auto lljit_builder = llvm::orc::LLJITBuilder();
    auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
    PRAJNA_VERIFY(JTMB);
    JTMB->getOptions().AllowFPOpFusion = llvm::FPOpFusion::Fast;
    JTMB->getOptions().UnsafeFPMath = true;
    lljit_builder.setJITTargetMachineBuilder(*JTMB);

#ifdef __APPLE__
    // TODO(zhangzhimin): 目前不是用自定的ObjectLinkingLayer会存在未知问题
    lljit_builder.setObjectLinkingLayerCreator(
        [=](llvm::orc::ExecutionSession &ES, const llvm::Triple &TT) {
            auto ll = std::make_unique<llvm::orc::ObjectLinkingLayer>(ES);
            return std::move(ll);
        });
#endif
    // TODO(zhangzhimin): 下面的代码会导致程序崩溃， 但可以正确的打印出汇编代码
    //    lljit_builder.setObjectLinkingLayerCreator(
    //         [=](llvm::orc::ExecutionSession &ES,  const llvm::Triple &TT) {
    //             auto ObjLinkingLayer = std::make_unique<llvm::orc::ObjectLinkingLayer>(ES);
    //             auto ObjTransformLayer =
    //               std::make_unique<llvm::orc::ObjectTransformLayer>(ES, *ObjLinkingLayer);
    //             ObjTransformLayer->setTransform([](std::unique_ptr<llvm::MemoryBuffer> Buf)
    //             -> llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>> {
    //                 std::ofstream out(".tmp.o", std::ios::binary);
    //                 out.write(Buf->getBufferStart(), Buf->getBufferSize());
    //                 out.close();
    //                 return Buf;
    //             });
    //           return ObjTransformLayer;
    //         });

    auto expect_up_lljit = lljit_builder.create();
    PRAJNA_VERIFY(expect_up_lljit);
    _up_lljit = std::move(*expect_up_lljit);

    _up_lljit->getMainJITDylib().addGenerator(
        cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            _up_lljit->getDataLayout().getGlobalPrefix())));
}

bool ExecutionEngine::LoadDynamicLib(std::string lib_name) {
    return llvm::sys::DynamicLibrary::getPermanentLibrary(lib_name.c_str()).isValid();
}

int64_t ExecutionEngine::GetValue(std::string name) {
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

    for (auto [ir_target, ir_sub_module] : ir_module->modules) {
        if (not ir_sub_module) continue;

        if (std::ranges::none_of(ir_sub_module->functions,
                                 [](std::shared_ptr<ir::Function> ir_function) {
                                     return ir_function->annotation_dict.count("kernel");
                                 })) {
            continue;
        }

        GpuCompiler gpu_compiler(ir_target);
        std::string hsaco_data;
        std::string file_base;
        if (ir_target == ir::Target::nvptx) {
            file_base =
                gpu_compiler.CompileToPTXCode(ir_sub_module->llvm_module, ir_sub_module->name);
        } else if (ir_target == ir::Target::amdgpu) {
            hsaco_data =
                gpu_compiler.CompileToHSACO(ir_sub_module->llvm_module, ir_sub_module->name);
        } else {
            PRAJNA_UNREACHABLE;
        }

        if (ir_target == ir::Target::nvptx) {
#ifdef _WIN32
            boost::dll::shared_library cuda_so("C:\\Windows\\System32\\nvcuda.dll");
#else
            boost::dll::shared_library cuda_so("/usr/lib/x86_64-linux-gnu/libcuda.so");
#endif

            auto cu_init = cuda_so.get<int(int)>("cuInit");
            cu_init(0);

            auto cu_library_load_from_file =
                cuda_so.get<int(int64_t *, const char *, int *, int *, int, int *, int *, int)>(
                    "cuLibraryLoadFromFile");
            auto cu_library_get_kernel =
                cuda_so.get<int(int64_t *, int64_t, const char *)>("cuLibraryGetKernel");

            int64_t cu_library = 0;
            auto cu_re2 =
                cu_library_load_from_file(&cu_library, fmt::format("{}.ptx", file_base).c_str(),
                                          nullptr, nullptr, 0, nullptr, nullptr, 0);
            PRAJNA_ASSERT(cu_re2 == 0, std::to_string(cu_re2));

            for (auto ir_function : ir_sub_module->functions) {
                if (ir_function->annotation_dict.count("kernel")) {
                    auto kernel_fun_address_name = GetKernelFunctionAddressName(ir_function);
                    auto test_kernel_fun =
                        reinterpret_cast<int64_t *>(this->GetValue(kernel_fun_address_name));
                    std::string function_name = MangleNvvmName(ir_function->fullname);
                    auto cu_re =
                        cu_library_get_kernel(test_kernel_fun, cu_library, function_name.c_str());
                    PRAJNA_ASSERT(cu_re == 0, std::to_string(cu_re));
                }
            }
        } else if (ir_target == ir::Target::amdgpu) {
            auto &hip_loader = HipRuntimeLoader::Instance();
            // 内核函数的地址存储到 JIT 环境中
            for (auto ir_function : ir_sub_module->functions) {
                if (ir_function->annotation_dict.count("kernel")) {
                    std::string function_name = MangleHipName(ir_function->fullname);
                    hipFunction_t kernel_func =
                        hip_loader.LoadKernelFunction(hsaco_data, function_name);
                    auto kernel_name_address_name = GetKernelFunctionAddressName(ir_function);
                    // 获取 JIT 环境中存储内核函数地址的指针。
                    int64_t *address =
                        reinterpret_cast<int64_t *>(this->GetValue(kernel_name_address_name));
                    PRAJNA_ASSERT(address,
                                  "Symbol not found in JIT env: " + kernel_name_address_name);

                    *address = static_cast<int64_t>(kernel_func);
                }
            }
        }
    }
}

void ExecutionEngine::BindCFunction(void *fun_ptr, std::string mangle_name) {
    llvm::orc::SymbolMap fun_symbol;
    auto fun_addr = llvm::orc::ExecutorAddr::fromPtr(fun_ptr);
    fun_symbol[_up_lljit->mangleAndIntern(mangle_name)] = llvm::orc::ExecutorSymbolDef(
        fun_addr, llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Absolute);
    exit_on_error(_up_lljit->getMainJITDylib().define(llvm::orc::absoluteSymbols(fun_symbol)));
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

#if defined(__linux__) || defined(WIN32)
    this->BindCFunction(reinterpret_cast<void *>(__truncdfhf2), "__truncdfhf2");
    this->BindCFunction(reinterpret_cast<void *>(__truncdfhf2), "__umodti3");
    this->BindCFunction(reinterpret_cast<void *>(__truncdfhf2), "__udivti3");
    this->BindCFunction(reinterpret_cast<void *>(__truncdfhf2), "__modti3");
    this->BindCFunction(reinterpret_cast<void *>(__truncdfhf2), "__divti3");
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
}

}  // namespace prajna::jit
