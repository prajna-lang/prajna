
add_library(prajna_config_target INTERFACE)
target_include_directories(prajna_config_target INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(prajna_config_target
    INTERFACE Boost::variant
    INTERFACE Boost::optional
    INTERFACE Boost::fusion
    INTERFACE Boost::algorithm
    INTERFACE fmt::fmt
)

if (PRAJNA_WITH_GPU)
target_compile_definitions(prajna_config_target INTERFACE PRAJNA_WITH_GPU)
endif()

target_compile_options(prajna_config_target
    INTERFACE -Wno-unused-parameter
    INTERFACE -Wno-deprecated-declarations
)

if (NOT PRAJNA_WITH_JUPYTER)
    target_compile_options(prajna_config_target
        INTERFACE -Werror
    )
endif()


# 需要禁止一些函数才能很好工作, mac下不支持detect-leaks
# 对于内存泄露来说, valgrind可能更加友好一点
if (PRAJNA_ENABLE_SANITIZE)
    target_compile_options(prajna_config_target
        INTERFACE -fsanitize=address
        INTERFACE -fno-omit-frame-pointer)

    target_link_options(prajna_config_target
        INTERFACE -fsanitize=address
        INTERFACE -fno-omit-frame-pointer
        INTERFACE -static-libsan)
endif()

# clang10 it works well
# Not work, it's overrided by -O3
# if wo use -O0 for parse, it works well on mac m1
# target_compile_options(prajna_config_target INTERFACE "$<$<CONFIG:RELEASE>:-O1>")

add_subdirectory(ast)
add_subdirectory(ir)
add_subdirectory(lowering)
add_subdirectory(parser)
add_subdirectory(codegen)
add_subdirectory(transform)
add_subdirectory(jit)
add_subdirectory(compiler)

add_library(prajna_core INTERFACE)

target_link_libraries(prajna_core
    INTERFACE prajna_config_target
    INTERFACE prajna_compiler
)
