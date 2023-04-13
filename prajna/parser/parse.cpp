#include "prajna/parser/parse.h"

#include "boost/spirit/home/classic/iterator/position_iterator.hpp"
#include "prajna/config.hpp"
#include "prajna/exception.hpp"
#include "prajna/helper.hpp"
#include "prajna/parser/grammar/statement_grammar.h"
#include "prajna/parser/lexer/code_lexer_config.hpp"

namespace prajna::parser {

bool parse(std::string code, prajna::ast::Statements& ast, std::string file_name,
           std::shared_ptr<Logger> logger) {
    grammar::ErrorHandler<iterator_type> error_handler(logger);
    grammar::SuccessHandler<iterator_type> success_handler(logger);
    CodeLexerType tokens;
    base_iterator_type first(RANGE(code), file_name);
    base_iterator_type last;
    iterator_type iter = tokens.begin(first, last);
    iterator_type end = tokens.end();

    prajna::parser::grammar::StatementGrammer<iterator_type, CodeLexerType> statement_grammar(
        tokens, error_handler, success_handler);

    try {
        auto skip = boost::spirit::qi::in_state("WS")[tokens.self];

        return boost::spirit::qi::phrase_parse(iter, end, statement_grammar, skip,
                                               boost::spirit::qi::skip_flag::postskip, ast);
    } catch (InvalidEscapeChar lexer_error) {
        logger->error("invalid escape char", lexer_error.source_position);
        return false;
    }
}

std::shared_ptr<prajna::ast::Statements> parse(std::string code, std::string file_name,
                                               std::shared_ptr<Logger> logger) {
    auto ast = std::make_shared<prajna::ast::Statements>();
    auto re = parse(code, *ast, file_name, logger);
    PRAJNA_ASSERT(re);
    return ast;
}

}  // namespace prajna::parser
