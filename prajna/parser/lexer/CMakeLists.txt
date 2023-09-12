add_library(prajna_lexer OBJECT code_lexer.cpp)
target_link_libraries(prajna_lexer
    PUBLIC prajna_config_target
    PUBLIC prajna_ast
    PUBLIC Boost::spirit
)

if (NOT MSVC) 
    target_compile_options(prajna_lexer INTERFACE "$<$<CONFIG:RELEASE>:-O0;-finline>")
endif()
