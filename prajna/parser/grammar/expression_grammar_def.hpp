
#include "prajna/parser/grammar/assign_to.hpp"
#include "prajna/parser/grammar/expression_grammar.h"

namespace prajna::parser::grammar {

template <typename Iterator, typename Lexer>
ExpressionGrammer<Iterator, Lexer>::ExpressionGrammer(
    const Lexer& tok, issue_handler<base_iterator_type, Iterator>& eh)
    : base_type(expr) {
    namespace phoenix = boost::phoenix;

    using qi::_val;
    using qi::as;
    using qi::omit;
    using qi::repeat;

    // @ref
    // https://www.boost.org/doc/libs/1_78_0/libs/spirit/doc/html/spirit/qi/quick_reference/compound_attribute_rules.html
    // 属性规则请参阅上述文档, 直接从tok到rule的转换报错会很复杂,
    // 拿不准的情况下可以多用qi::as来逐步确定哪里出了问题

    identifier = tok.identifier;
    int_literal = tok.int_literal;
    string_literal = tok.string_literal;

    expr = logical_expr.alias();

    type_postfix_operator =
        as<ast::PostfixTypeOperator>()[(omit[tok.l_s_bracket] > (identifier | int_literal) >
                                        tok.r_s_bracket)] |
        tok.times;
    type = identifiers_resolution >> *type_postfix_operator;

    template_argument = type | tok.int_literal;
    template_arguments = omit[tok.less] >> template_argument % tok.comma >> omit[tok.greater];
    identifier_template = identifier >> -template_arguments;
    identifiers_resolution = -tok.scope >> identifier_template % tok.scope;

    // TODO(zhangzhimin)
    // 优先级和rust和c++不同， prajna的逻辑运算未区分&& 和 || 的优先级，
    // 个人认为使用()来明确区分优先级更好，用户无法清晰记住复杂的优先级顺序，越简单越好
    logical_op = tok.and_ | tok.or_ | tok.xor_;
    logical_expr = equality_expr >> *(logical_op > equality_expr);

    equality_op = tok.equal | tok.not_equal;
    equality_expr = relational_expr >> *(equality_op > relational_expr);

    relational_op = tok.less | tok.less_or_equal | tok.greater | tok.greater_or_equal;
    relational_expr = additive_expr >> *(relational_op > additive_expr);

    additive_op = tok.plus | tok.minus;
    additive_expr = multiplicative_expr >> *(additive_op > multiplicative_expr);

    multiplicative_op = tok.times | tok.divide | tok.remain;
    multiplicative_expr = unary_expr >> *(multiplicative_op > unary_expr);
    // TODO 需要对错误信息, 二次处理, 时期更加有效
    // name属性在on_fail的时候是需要的
    multiplicative_expr.name("multiplicative_expr");

    unary_op = tok.times | tok.and_ | tok.plus | tok.minus | tok.not_;
    unary_expr = kernel_function_call | (unary_op > unary_expr);

    kernel_function_call =
        access_call_index_expr >>
        -(tok.left_angle_brackets > expr > tok.comma > expr > tok.right_angle_brackets >
          omit[tok.l_bracket] > as<ast::Expressions>()[-(expr % tok.comma)] > tok.r_bracket);

    member_access = tok.period > identifier;
    call = tok.l_bracket > as<ast::Expressions>()[-(expr % tok.comma)] > tok.r_bracket;
    index = tok.l_s_bracket > expr > tok.r_s_bracket;

    access_call_index_expr = primary_expr >> *(member_access | call | index);

    array = omit[tok.l_s_bracket] > (expr % tok.comma) > tok.r_s_bracket;

    primary_expr = literal | tok.this_ | identifiers_resolution | cast | sizeof_ | array |
                   omit[tok.l_bracket] > expr > omit[tok.r_bracket];

    cast = tok.cast >> omit[tok.less] >> type >> omit[tok.greater] >> omit[tok.l_bracket] > expr >
           omit[tok.r_bracket];

    sizeof_ = tok.sizeof_ > omit[tok.l_bracket] > type > tok.r_bracket;

    literal = int_literal_postfix | float_literal_postfix | tok.string_literal | tok.float_literal |
              tok.int_literal | tok.true_literal | tok.false_literal | tok.char_literal;

    int_literal_postfix = int_literal >> identifier;
    float_literal_postfix = tok.float_literal >> identifier;

    typedef phoenix::function<issue_handler<base_iterator_type, Iterator>&> issue_handler_function;
    qi::on_error<qi::fail>(expr, issue_handler_function(eh)(qi::_1, qi::_3, qi::_4));
}

}  // namespace prajna::parser::grammar

