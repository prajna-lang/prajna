#include "prajna/parser/grammar/assign_to.hpp"
#include "prajna/parser/grammar/statement_grammar.h"

namespace prajna::parser::grammar {

template <typename Iterator, typename Lexer>
StatementGrammer<Iterator, Lexer>::StatementGrammer(const Lexer& tok,
                                                    issue_handler<base_iterator_type, Iterator>& eh)
    : base_type(statements), expr(tok, eh) {
    using qi::_1;
    using qi::_2;
    using qi::_3;
    using qi::_4;
    using qi::_val;
    using qi::as;
    using qi::omit;

    typedef boost::phoenix::function<issue_handler<base_iterator_type, Iterator>>
        issue_handler_function;

    // @ref
    // https://www.boost.org/doc/libs/1_78_0/libs/spirit/doc/html/spirit/qi/quick_reference/compound_attribute_rules.html
    // 属性规则请参阅上述文档, 直接从tok到rule的转换报错会很复杂,
    // 拿不准的情况下可以多用qi::as来逐步确定哪里出了问题

    statements = *statement;

    import = tok.import > identifiers_resolution >> -(tok.as > identifier) > tok.semicolon;
    export_ = tok.export_ > identifier > tok.semicolon;

    statement = block | if_ | return_ | import | export_ | struct_ | interface |
                implement_struct | implement_struct_for_interface | function |
                while_ | for_ | break_ | continue_ | variable_declaration |
                assignment | expr > tok.semicolon;
    block = (tok.l_braces > *statement > tok.r_braces);

    identifier = tok.identifier;

    type = expr.type;
    identifiers_resolution = expr.identifiers_resolution;

    variable_declaration =
        tok.var >> identifier >> -(tok.colon >> type) >> -(tok.assign >> expr) >> tok.semicolon;

    // // Assignment = expr > tok.assign > expr > tok.semicolon;  //!error, because conflict with
    // expr
    assignment = expr >> tok.assign >> expr >> tok.semicolon;

    return_ = tok.return_ > (-expr) > tok.semicolon;
    break_ = tok.break_ > tok.semicolon;
    continue_ = tok.continue_ > tok.semicolon;

    if_ = (tok.if_ > expr > block > -(tok.else_ > block));

    while_ = (tok.while_ > expr > block);

    annotation = tok.at > identifier >
                 -(omit[tok.l_bracket] > -(as<std::string>()[expr.string_literal] % tok.comma) >
                   tok.r_bracket);
    annotations = *annotation;

    // for_ = annotations >> tok.for_ > identifier > tok.in > expr > block;

    for_ = annotations >> tok.for_ > identifier > tok.in > expr > tok.to >
           expr > block;

    field = as<ast::Field>()[identifier > tok.colon > type > tok.semicolon];
    template_parameter = as<ast::TemplateParameter>()[identifier > tok.colon > tok.template_];
    template_parameters = omit[tok.less] > template_parameter % tok.comma > omit[tok.greater];
    struct_ = annotations >> tok.struct_ > identifier > -template_parameters > tok.l_braces >
              as<std::vector<prajna::ast::Field>>()[*field] > tok.r_braces;

    interface = tok.interface > identifier > tok.l_braces > *(function) > tok.r_braces;
    implement_struct_for_interface =
        tok.implement > type > tok.for_ > type > tok.l_braces > *(function) > tok.r_braces;
    implement_struct = tok.implement > identifiers_resolution > -template_parameters >
                       tok.l_braces > *(function) > tok.r_braces;

    parameter = identifier > tok.colon > type;
    parameters = omit[tok.l_bracket] > -(parameter % tok.comma) > tok.r_bracket;
    function_header = annotations >> tok.func > identifier > parameters > -(tok.arrow > type);

    binary_operator =
        as<ast::Identifier>()[tok.equal | tok.not_equal | tok.less | tok.less_or_equal |
                              tok.greater | tok.greater_or_equal | tok.plus | tok.minus |
                              tok.times | tok.divide | tok.remain | tok.shift_left |
                              tok.shift_right | tok.and_ | tok.or_ | tok.xor_ | tok.not_];
    unary_operator =
        as<ast::Identifier>()[as<std::vector<ast::Operator>>()[tok.backslash >
                                                               (tok.plus | tok.minus | tok.times |
                                                                tok.and_ | tok.not_)]];
    operator_header = annotations >> tok.operator_ > (as<ast::Identifier>()[unary_operator] |
                                                      as<ast::Identifier>()[binary_operator]) >
                      parameters > -(tok.arrow > type);
    function = (function_header | operator_header) >> (block | tok.semicolon);

    qi::on_error<qi::fail>(statements, issue_handler_function(eh)(_1, _3, _4));
}

}  // namespace prajna::parser::grammar
