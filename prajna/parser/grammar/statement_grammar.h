#pragma once

#include <vector>

#include "boost/spirit/include/lex_lexertl.hpp"
#include "boost/spirit/include/qi.hpp"
#include "prajna/ast/ast.hpp"
#include "prajna/parser/grammar/expression_grammar.h"
#include "prajna/parser/grammar/handler.hpp"

namespace prajna::parser::grammar {

namespace qi = boost::spirit::qi;
namespace lex = boost::spirit::lex;
namespace ascii = boost::spirit::ascii;

template <typename Iterator, typename Lexer>
struct StatementGrammer
    : qi::grammar<Iterator, ast::Statements(), qi::in_state_skipper<typename Lexer::lexer_def>> {
    typedef qi::grammar<Iterator, ast::Statements(),
                        qi::in_state_skipper<typename Lexer::lexer_def>>
        base_type;

    typedef Iterator iterator_type;
    typedef qi::in_state_skipper<typename Lexer::lexer_def> skipper_type;
    ExpressionGrammer<Iterator, Lexer> expr;

    __attribute__((no_sanitize("address")))
    StatementGrammer(const Lexer &tok, ErrorHandler<Iterator> error_handler,
                     SuccessHandler<Iterator> success_handler);

    template <typename _T>
    using rule = qi::rule<Iterator, _T(), skipper_type>;

    rule<ast::Type> type;
    rule<ast::Identifier> identifier;
    rule<ast::IdentifierPath> identifier_path;
    rule<ast::Use> use;
    rule<ast::Module> module_;
    rule<ast::Statements> statements;
    rule<ast::Block> block;
    rule<ast::Statement> statement;
    rule<ast::Statement> single_statement;
    rule<ast::Blank> semicolon_statement;
    rule<ast::Pragma> pragma;
    rule<ast::Annotation> annotation;
    rule<ast::AnnotationDict> annotation_dict;
    rule<ast::VariableDeclaration> variable_declaration;
    rule<ast::Assignment> assignment;
    rule<ast::Return> return_;
    rule<ast::Break> break_;
    rule<ast::Continue> continue_;
    rule<ast::If> if_;
    rule<ast::While> while_;
    rule<ast::For> for_;

    rule<ast::Field> field;
    rule<std::list<ast::Field>> fields;
    rule<ast::TemplateParameter> template_parameter;
    rule<ast::TemplateParameters> template_parameters;
    rule<ast::Struct> struct_;

    rule<std::list<ast::Function>> functions;
    rule<ast::InterfacePrototype> interface;
    rule<ast::ImplementType> implement_type;
    rule<ast::ImplementInterfaceForType> implement_interface;

    rule<ast::Parameter> parameter;
    rule<ast::Parameters> parameters;
    rule<boost::optional<ast::Block>> function_implement;
    rule<ast::FunctionHeader> function_header;
    rule<ast::Function> function;

    rule<ast::TemplateAbleStatement> templateable_statement;
    rule<ast::Template> template_;
    rule<ast::TemplateStatement> template_statement;
};

}  // namespace prajna::parser::grammar
