add_library(prajna_codegen
    llvm_codegen.cpp
)

target_link_libraries(prajna_codegen
    PUBLIC prajna_ir
    PUBLIC llvm_include_dir
    PRIVATE LLVMExecutionEngine
    PRIVATE LLVMCodeGen
    PRIVATE LLVMAnalysis
    PRIVATE LLVMTarget
    PRIVATE LLVMSupport
    PRIVATE LLVMX86Desc
    PRIVATE LLVMX86Info
    PRIVATE LLVMX86CodeGen
    PRIVATE LLVMX86AsmParser
    PRIVATE LLVMAArch64Desc # 不同平台不同
    PRIVATE LLVMAArch64Info
    PRIVATE LLVMAArch64CodeGen
    PRIVATE LLVMAArch64AsmParser
    PUBLIC LLVMCore # Module在头文件暴露了
)


if (PRAJNA_ENABLE_LLVM_DUMP)
    target_compile_definitions(prajna_codegen PRIVATE PRAJNA_ENABLE_LLVM_DUMP=1)
endif()
if (PRAJNA_ENABLE_LLVM_VERIFY)
    target_compile_definitions(prajna_codegen PRIVATE PRAJNA_ENABLE_LLVM_VERIFY=1)
endif()
