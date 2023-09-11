add_executable(prajna_compiler_tests
    compiler_tests.cpp
)

target_link_libraries(prajna_compiler_tests
    PUBLIC prajna_core
    PRIVATE gtest_main
    PUBLIC prajna_compiler
    PRIVATE clang_rt_whole
)
