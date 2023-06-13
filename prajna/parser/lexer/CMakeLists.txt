add_library(prajna_lexer code_lexer.cpp)
target_link_libraries(prajna_lexer
    PUBLIC prajna_config_target
    PUBLIC prajna_ast
    PUBLIC Boost::spirit
)

# work with clang10, -O0 work with clang14
# TODO: 需要修复, 还是需要将一些必要的优化加回来
target_compile_options(prajna_lexer INTERFACE "$<$<CONFIG:RELEASE>:-O0;-finline>")
