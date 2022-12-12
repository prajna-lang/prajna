//
//  prajna_parser.cpp
//  binding
//
//  Created by 张志敏 on 11/13/13.
//
//

#include "prajna/parser/parse.h"

#include "boost/spirit/home/classic/iterator/position_iterator.hpp"
#include "prajna/config.hpp"
#include "prajna/helper.hpp"
#include "prajna/parser/grammar/statement_grammar.h"
#include "prajna/parser/lexer/code_lexer_config.hpp"

namespace prajna::parser {

bool parse(std::string code, prajna::ast::Statements& ast, std::string file_name,
           std::shared_ptr<Logger> logger) {
    typedef grammar::issue_handler<base_iterator_type, iterator_type> issue_handler_type;

    issue_handler_type eh;
    CodeLexerType tokens;

    base_iterator_type first(RANGE(code), file_name);
    base_iterator_type last;

    iterator_type iter = tokens.begin(first, last);
    iterator_type end = tokens.end();

    prajna::parser::grammar::StatementGrammer<iterator_type, CodeLexerType> program(tokens, eh);
    auto t = boost::spirit::qi::in_state("WS")[tokens.self];

    return boost::spirit::qi::phrase_parse(iter, end, program, t, ast);
}

std::shared_ptr<prajna::ast::Statements> parse(std::string code, std::string file_name,
                                               std::shared_ptr<Logger> logger) {
    auto ast = std::make_shared<prajna::ast::Statements>();
    auto re = parse(code, *ast, file_name, logger);
    if (re) {
        return ast;
    } else {
        return nullptr;
    }
}

}  // namespace prajna::parser
