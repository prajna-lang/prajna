#pragma once

#include <vector>

#include "boost/spirit/include/lex_lexertl.hpp"
#include "boost/spirit/include/qi.hpp"
#include "boost/tuple/tuple.hpp"
#include "prajna/ast/ast.hpp"
#include "prajna/parser/grammar/handler.hpp"
#include "prajna/parser/lexer/code_lexer_config.hpp"

namespace prajna::parser::grammar {

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template <typename Iterator, typename Lexer>
struct ExpressionGrammer
    : qi::grammar<Iterator, ast::Expression(), qi::in_state_skipper<typename Lexer::lexer_def>> {
    typedef qi::grammar<Iterator, ast::Expression(),
                        qi::in_state_skipper<typename Lexer::lexer_def>>
        base_type;

    typedef Iterator iterator_type;
    typedef qi::in_state_skipper<typename Lexer::lexer_def> skipper_type;

    __attribute__((no_sanitize("address")))
    ExpressionGrammer(const Lexer& tok, ErrorHandler<Iterator> error_handler,
                      SuccessHandler<Iterator> success_handler);

    template <typename _T>
    using rule = qi::rule<Iterator, _T(), skipper_type>;

    rule<ast::Expression> expr;

    rule<ast::Identifier> identifier;
    rule<ast::IntLiteral> int_literal;
    rule<ast::IntLiteralPostfix> int_literal_postfix;
    rule<ast::FloatLiteralPostfix> float_literal_postfix;
    rule<ast::StringLiteral> string_literal;
    rule<ast::PostfixTypeOperator> type_array_postfix_operator;
    rule<ast::PostfixTypeOperator> type_postfix_operator;
    rule<ast::BasicType> basic_type;
    rule<ast::FunctionType> function_type;
    rule<ast::PostfixType> type;
    rule<ast::TemplateArgument> template_argument;
    rule<ast::IdentifierPath> identifier_path;
    rule<ast::TemplateArguments> template_arguments;
    rule<ast::TemplateIdentifier> template_identifier;

    rule<ast::Operator> logical_op;
    rule<ast::Operator> equality_op;
    rule<ast::Operator> relational_op;
    rule<ast::Operator> additive_op;
    rule<ast::Operator> multiplicative_op;
    rule<ast::Operator> unary_op;
    rule<ast::Expression> logical_expr;
    rule<ast::Expression> equality_expr;
    rule<ast::Expression> relational_expr;
    rule<ast::Expression> additive_expr;
    rule<ast::Expression> multiplicative_expr;
    rule<ast::Operand> unary_expr;
    rule<ast::Array> array;
    rule<ast::Expressions> arguments;
    rule<ast::BinaryOperation> member_access;
    rule<ast::BinaryOperation> call;
    rule<ast::BinaryOperation> index;
    rule<ast::Expression> access_call_index_expr;
    rule<ast::Cast> cast;
    rule<ast::Operand> primary_expr;
    rule<ast::Operand> literal;
    rule<ast::SizeOf> sizeof_;
    rule<ast::KernelFunctionCall> kernel_function_call;
    rule<ast::Operand> kernel_function_operand;
    rule<ast::DynamicCast> dynamic_cast_;
};

}  // namespace prajna::parser::grammar
