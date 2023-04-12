add_library(prajna_lexer code_lexer.cpp)
target_link_libraries(prajna_lexer
    PUBLIC prajna_config_target
    PUBLIC prajna_ast
    PUBLIC Boost::spirit
)

if (PRAJNA_SPIRIT_DEBUG)
    target_compile_definitions(prajna_lexer PRIVATE BOOST_SPIRIT_DEBUG)
endif()

# work with clang10, -O0 work with clang14
target_compile_options(prajna_lexer INTERFACE "$<$<CONFIG:RELEASE>:-O1;-finline>")
