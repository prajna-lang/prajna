add_library(prajna_jit OBJECT
    execution_engine.cpp
)

add_dependencies(prajna_jit llc)

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
    PRIVATE LLVMSupport
    PRIVATE LLVMX86Desc
    PRIVATE LLVMX86Info
    PRIVATE LLVMAArch64Info
    PRIVATE LLVMAArch64CodeGen
    PRIVATE LLVMAArch64AsmParser
    PRIVATE LLVMAArch64Desc
    PRIVATE LLVMCore
)

if (UNIX AND NOT APPLE)
    target_link_libraries(prajna_jit PUBLIC clang_rt.builtins-${CMAKE_SYSTEM_PROCESSOR})
endif()

if (MSVC) 
    add_library(clang_rt_whole INTERFACE)
    set_property(TARGET clang_rt_whole PROPERTY INTERFACE_LINK_LIBRARIES
    -WHOLEARCHIVE:$<TARGET_FILE:clang_rt.builtins-x86_64>)
    add_dependencies(clang_rt_whole clang_rt.builtins-x86_64)
    target_link_libraries(prajna_jit PUBLIC -WHOLEARCHIVE:$<TARGET_FILE:clang_rt.builtins-x86_64>)
endif()

# https://github.com/fxcoudert/gfortran-for-macOS/releases
# 对于mac平台, 需要下载安装gfortran. 因为jigt运行时float128会依赖下面的动态库
if (APPLE AND UNIX)
    # target_link_libraries(prajna_jit PRIVATE /usr/local/gfortran/lib/libgfortran.dylib)
endif()

target_include_directories(prajna_jit PRIVATE /usr/local/cuda/include)
