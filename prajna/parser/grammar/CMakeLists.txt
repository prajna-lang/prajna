add_library(prajna_grammar OBJECT statement_grammar.cpp expression_grammar.cpp)
target_link_libraries(prajna_grammar
    PUBLIC prajna_ast
    PRIVATE Boost::spirit
    PUBLIC prajna_lexer
)
