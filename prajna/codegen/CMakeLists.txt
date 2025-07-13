add_library(prajna_codegen OBJECT
    llvm_codegen.cpp
)

include(CheckCXXCompilerFlag)

# Detect x86 and AArch64 platforms
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|i[3-6]86|amd64")
    set(PRAJNA_TARGET_X86 ON)
    set(LLVMNativeCodeGen LLVMX86CodeGen)
    set(LLVMNativeAsmParser LLVMX86AsmParser)
    set(LLVMNativeDesc LLVMX86Desc)
    set(LLVMNativeInfo LLVMX86Info)
    message(STATUS "Detected x86 architecture")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|AARCH64|ARM64|arm64")
    set(PRAJNA_TARGET_AARCH64 ON)
    set(LLVMNativeCodeGen LLVMAArch64CodeGen)
    set(LLVMNativeAsmParser LLVMAArch64AsmParser)
    set(LLVMNativeDesc LLVMAArch64Desc)
    set(LLVMNativeInfo LLVMAArch64Info)
    message(STATUS "Detected AArch64 architecture")
else()
    message(STATUS "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()


target_link_libraries(prajna_codegen
    PUBLIC prajna_ir
    PRIVATE llvm_include_dir
    PRIVATE LLVMDebugInfoCodeView
    PRIVATE LLVMExecutionEngine
    PRIVATE LLVMCodeGen
    PRIVATE LLVMAnalysis
    PRIVATE LLVMTarget
    # PUBLIC LLVMSupport # 外部会用到, 后面有时间在处理, 应该需要-DLLVM_ENABLE_ABI_BREAKING_CHECKS=OFF才可以关闭
    PRIVATE ${LLVMNativeCodeGen}
    PRIVATE ${LLVMNativeAsmParser}
    PRIVATE ${LLVMNativeDesc}
    PRIVATE ${LLVMNativeInfo}
    PUBLIC LLVMPasses
    PUBLIC LLVMCore # Module在头文件暴露了
)
