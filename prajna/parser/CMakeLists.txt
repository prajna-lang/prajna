add_subdirectory(lexer)
add_subdirectory(grammar)

add_library(prajna_parser OBJECT parse.cpp)
target_link_libraries(prajna_parser
    PUBLIC prajna_ast
    PUBLIC prajna_grammar
)
