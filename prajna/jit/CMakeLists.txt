add_library(prajna_jit OBJECT
    execution_engine.cpp
)


target_link_libraries(prajna_jit
    PUBLIC prajna_config_target
    PRIVATE llvm_include_dir
    PRIVATE LLVMExecutionEngine
    PRIVATE LLVMIRReader
    PRIVATE LLVMJITLink
    PUBLIC LLVMOrcJIT
    PRIVATE LLVMMCJIT
    PRIVATE LLVMX86CodeGen
    PRIVATE LLVMX86AsmParser
    PRIVATE LLVMCodeGen
    PRIVATE LLVMAnalysis
    PRIVATE LLVMTarget
    PRIVATE ${LLVMNativeCodeGen}
    PRIVATE ${LLVMNativeAsmParser}
    PRIVATE ${LLVMNativeDesc}
    PRIVATE ${LLVMNativeInfo}
    PRIVATE LLVMCore
    PUBLIC LLVMLinker

)

if (UNIX AND NOT APPLE)
    target_link_libraries(prajna_jit PUBLIC clang_rt.builtins-${CMAKE_SYSTEM_PROCESSOR})
endif()

if (MSVC)

    add_library(clang_rt_whole INTERFACE)
    target_link_libraries(clang_rt_whole INTERFACE "$<LINK_LIBRARY:WHOLE_ARCHIVE,clang_rt.builtins-x86_64>")
    target_link_libraries(prajna_jit PRIVATE clang_rt_whole)
endif()

# https://github.com/fxcoudert/gfortran-for-macOS/releases
# 对于mac平台, 需要下载安装gfortran. 因为jigt运行时float128会依赖下面的动态库
if (APPLE AND UNIX)
    # target_link_libraries(prajna_jit PRIVATE /usr/local/gfortran/lib/libgfortran.dylib)
endif()


if (PRAJNA_WITH_CUDA)
    target_link_libraries(prajna_jit
        PRIVATE LLVMNVPTXCodeGen  # 添加 NVPTX 后端支持
        PRIVATE LLVMNVPTXDesc     # NVPTX 目标描述
        PRIVATE LLVMNVPTXInfo     # NVPTX 目标信息
        PRIVATE LLVMAsmPrinter    # 支持 PTX 汇编输出
    )
endif()

if (PRAJNA_WITH_ROCM)
    target_link_libraries(prajna_jit
        PRIVATE LLVMAMDGPUCodeGen  # 添加 HSACO 后端支持
        PRIVATE LLVMAMDGPUDesc     # HSACO 目标描述
        PRIVATE LLVMAMDGPUInfo     # HSACO 目标信息
        PRIVATE LLVMAsmPrinter    # 支持 HSACO 汇编输出
    )
endif()
