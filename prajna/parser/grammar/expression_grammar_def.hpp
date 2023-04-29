#pragma once

#include "prajna/parser/grammar/assign_to.hpp"
#include "prajna/parser/grammar/expression_grammar.h"

namespace prajna::parser::grammar {

template <typename Iterator, typename Lexer>
ExpressionGrammer<Iterator, Lexer>::ExpressionGrammer(const Lexer& tok,
                                                      ErrorHandler<Iterator> error_handler,
                                                      SuccessHandler<Iterator> success_handler)
    : base_type(expr) {
    using qi::_1;
    using qi::_2;
    using qi::_3;
    using qi::_4;
    using qi::_val;
    using qi::as;
    using qi::expect;
    using qi::fail;
    using qi::omit;
    using qi::on_error;
    using qi::on_success;
    using qi::repeat;

    // @ref
    // https://www.boost.org/doc/libs/1_78_0/libs/spirit/doc/html/spirit/qi/quick_reference/compound_attribute_rules.html
    // 属性规则请参阅上述文档, 直接从tok到rule的转换报错会很复杂, 故对于复杂的规则我们单独写一个rule
    // 拿不准的情况下可以多用qi::as来逐步确定哪里出了问题

    auto error_handler_function =
        boost::phoenix::function<ErrorHandler<Iterator>>(error_handler)(_1, _2, _3, _4);

    auto success_handler_function =
        boost::phoenix::function<SuccessHandler<Iterator>>(success_handler)(_1, _2, _3, _val);

    expr.name("expression");
    expr = equality_expr.alias();
    on_error<fail>(expr, error_handler_function);
    on_success(expr, success_handler_function);

    equality_op = tok.equal | tok.not_equal;
    equality_expr.name("expression");
    equality_expr = logical_expr >> *(equality_op > logical_expr);
    on_error<fail>(equality_expr, error_handler_function);
    on_success(equality_expr, success_handler_function);

    logical_op = tok.and_ | tok.or_ | tok.xor_;
    logical_expr.name("expression");
    logical_expr = relational_expr >> *(logical_op > relational_expr);
    on_error<fail>(logical_expr, error_handler_function);
    on_success(logical_expr, success_handler_function);

    relational_op = tok.less | tok.less_or_equal | tok.greater | tok.greater_or_equal;
    relational_expr.name("expression");
    relational_expr = additive_expr >> *(relational_op > additive_expr);
    on_error<fail>(relational_expr, error_handler_function);
    on_success(relational_expr, success_handler_function);

    additive_op = tok.plus | tok.minus;
    additive_expr.name("expression");
    additive_expr = multiplicative_expr >> *(additive_op > multiplicative_expr);
    on_error<fail>(additive_expr, error_handler_function);
    on_success(additive_expr, success_handler_function);

    multiplicative_op = tok.times | tok.divide | tok.remain;
    multiplicative_expr.name("expression");
    multiplicative_expr = unary_expr >> *(multiplicative_op > unary_expr);
    on_error<fail>(multiplicative_expr, error_handler_function);
    on_success(multiplicative_expr, success_handler_function);

    unary_op = tok.times | tok.and_ | tok.plus | tok.minus | tok.not_;
    unary_expr.name("unary expression");
    unary_expr = kernel_function_call | (unary_op > unary_expr);
    on_error<fail>(unary_expr, error_handler_function);
    on_success(unary_expr, success_handler_function);

    kernel_function_call.name("kernel function call");
    kernel_function_call =
        access_call_index_expr >> -(tok.left_arrow2 > expr > tok.comma > expr > tok.right_arrow2 >
                                    omit[tok.left_bracket] > arguments > tok.right_bracket);
    on_error<fail>(kernel_function_call, error_handler_function);
    on_success(kernel_function_call, success_handler_function);

    access_call_index_expr.name("access/call/index expression");
    access_call_index_expr = primary_expr >> *(member_access | call | index);
    on_error<fail>(access_call_index_expr, error_handler_function);
    on_success(access_call_index_expr, success_handler_function);

    member_access.name("member access");
    member_access = tok.period > identifier_path;
    on_error<fail>(member_access, error_handler_function);
    on_success(member_access, success_handler_function);

    call.name("call");
    call = tok.left_bracket > arguments > tok.right_bracket;
    on_error<fail>(call, error_handler_function);
    on_success(call, success_handler_function);

    index.name("index");
    index = tok.left_square_bracket > arguments > tok.right_square_bracket;
    on_error<fail>(index, error_handler_function);
    on_success(index, success_handler_function);

    arguments.name("arguments");
    arguments = as<ast::Expressions>()[-(expr % tok.comma)];
    on_error<fail>(arguments, error_handler_function);
    on_success(arguments, success_handler_function);

    primary_expr.name("primary experssion");
    primary_expr = literal | tok.this_ | identifier_path | array |
                   omit[tok.left_bracket] > expr > omit[tok.right_bracket];
    on_error<fail>(primary_expr, error_handler_function);
    on_success(primary_expr, success_handler_function);

    array.name("array");
    array = omit[tok.left_square_bracket] > (expr % tok.comma) > tok.right_square_bracket;
    on_error<fail>(array, error_handler_function);
    on_success(array, success_handler_function);

    literal.name("literal");
    literal = int_literal_postfix | float_literal_postfix | tok.string_literal | tok.float_literal |
              tok.int_literal | tok.true_literal | tok.false_literal | tok.char_literal;
    on_error<fail>(literal, error_handler_function);
    on_success(literal, success_handler_function);

    int_literal_postfix.name("int literal with postfix");
    int_literal_postfix = int_literal >> identifier;
    on_error<fail>(int_literal_postfix, error_handler_function);
    on_success(int_literal_postfix, success_handler_function);

    float_literal_postfix.name("float literal with postfix");
    float_literal_postfix = tok.float_literal >> identifier;
    on_error<fail>(float_literal_postfix, error_handler_function);
    on_success(float_literal_postfix, success_handler_function);

    identifier_path.name("identifier path");
    identifier_path = -tok.scope >> template_identifier % tok.scope;
    on_error<fail>(identifier_path, error_handler_function);
    on_success(identifier_path, success_handler_function);

    template_identifier.name("identifier with templates");
    template_identifier = identifier >> -template_arguments;
    on_error<fail>(template_identifier, error_handler_function);
    on_success(template_identifier, success_handler_function);

    template_arguments.name("template arguments");
    template_arguments = omit[tok.less] >> template_argument % tok.comma >> omit[tok.greater];
    on_error<fail>(template_arguments, error_handler_function);
    on_success(template_arguments, success_handler_function);

    template_argument.name("template argument");
    template_argument = type | tok.int_literal;
    on_error<fail>(template_argument, error_handler_function);
    on_success(template_argument, success_handler_function);

    type.name("type");
    type = identifier_path.alias();

    identifier.name("identifier");
    identifier = tok.identifier;
    on_error<fail>(identifier, error_handler_function);
    on_success(identifier, success_handler_function);

    int_literal.name("int literal");
    int_literal = tok.int_literal;
    on_error<fail>(int_literal, error_handler_function);
    on_success(int_literal, success_handler_function);

    string_literal.name("string literal");
    string_literal = tok.string_literal;
    on_error<fail>(string_literal, error_handler_function);
    on_success(string_literal, success_handler_function);
}

}  // namespace prajna::parser::grammar
