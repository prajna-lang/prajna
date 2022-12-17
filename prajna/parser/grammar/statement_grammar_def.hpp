#pragma once

#include "prajna/parser/grammar/assign_to.hpp"
#include "prajna/parser/grammar/statement_grammar.h"

namespace prajna::parser::grammar {

template <typename Iterator, typename Lexer>
StatementGrammer<Iterator, Lexer>::StatementGrammer(const Lexer &tok,
                                                    ErrorHandler<Iterator> error_handler,
                                                    SuccessHandler<Iterator> success_handler)
    : base_type(statements), expr(tok, error_handler, success_handler) {
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

    auto error_handler_function =
        boost::phoenix::function<ErrorHandler<Iterator>>(error_handler)(_1, _2, _3, _4);

    auto success_handler_function =
        boost::phoenix::function<SuccessHandler<Iterator>>(success_handler)(_1, _2, _3, _val);

    // @ref
    // https://www.boost.org/doc/libs/1_78_0/libs/spirit/doc/html/spirit/qi/quick_reference/compound_attribute_rules.html
    // 属性规则请参阅上述文档, 直接从tok到rule的转换报错会很复杂,
    // 拿不准的情况下可以多用qi::as来逐步确定哪里出了问题

    statements.name("statements");
    statements = *statement;
    on_error<fail>(statements, error_handler_function);
    on_success(statements, success_handler_function);

    statement.name("statement");
    statement = block | if_ | struct_ | interface | implement_struct |
                implement_struct_for_interface | function | while_ | for_ | single_statement |
                semicolon_statement;
    on_error<fail>(statement, error_handler_function);
    on_success(statement, success_handler_function);

    semicolon_statement.name("semicolon statement");
    semicolon_statement = +tok.semicolon;
    on_error<fail>(semicolon_statement, error_handler_function);
    on_success(semicolon_statement, success_handler_function);

    block.name("block");
    block = (tok.l_braces > *statement > tok.r_braces);
    on_error<fail>(block, error_handler_function);
    on_success(block, success_handler_function);

    single_statement.name("single_statement");
    single_statement = (return_ | import | export_ | break_ | continue_ | variable_declaration |
                        assignment | expr) > tok.semicolon;
    on_error<fail>(single_statement, error_handler_function);
    on_success(single_statement, success_handler_function);

    import.name("import");
    import = tok.import > identifier_path >> -(tok.as > identifier);
    on_error<fail>(import, error_handler_function);
    on_success(import, success_handler_function);

    export_.name("export");
    export_ = tok.export_ > identifier;
    on_error<fail>(export_, error_handler_function);
    on_success(export_, success_handler_function);

    variable_declaration.name("variable_declaration");
    variable_declaration =
        (tok.var > identifier) >> -(tok.colon > type) >> -(omit[tok.assign] > expr);
    on_error<fail>(variable_declaration, error_handler_function);
    on_success(variable_declaration, success_handler_function);

    // // Assignment = expr > tok.assign > expr > tok.semicolon;  //!error, because conflict with
    // expr
    assignment.name("assignment");
    assignment = expr >> omit[tok.assign] > expr;
    on_error<fail>(assignment, error_handler_function);
    on_success(assignment, success_handler_function);

    return_.name("return");
    return_ = tok.return_ > (-expr);
    on_error<fail>(return_, error_handler_function);
    on_success(return_, success_handler_function);

    break_.name("break");
    break_ = tok.break_;
    on_error<fail>(break_, error_handler_function);
    on_success(break_, success_handler_function);

    continue_.name("continue");
    continue_ = tok.continue_;
    on_error<fail>(continue_, error_handler_function);
    on_success(continue_, success_handler_function);

    if_.name("if");
    if_ = (tok.if_ > expr > block > -(tok.else_ > block));
    on_error<fail>(if_, error_handler_function);
    on_success(if_, success_handler_function);

    while_.name("while");
    while_ = (tok.while_ > expr > block);
    on_error<fail>(while_, error_handler_function);
    on_success(while_, success_handler_function);

    for_.name("for");
    for_ = annotations >> tok.for_ > identifier > tok.in > expr > tok.to > expr > block;
    on_error<fail>(for_, error_handler_function);
    on_success(for_, success_handler_function);

    struct_.name("struct");
    struct_ = annotations >> tok.struct_ > identifier > -template_parameters > tok.l_braces >
              as<std::vector<prajna::ast::Field>>()[*field] > tok.r_braces;
    on_error<fail>(struct_, error_handler_function);
    on_success(struct_, success_handler_function);

    field.name("field");
    field = as<ast::Field>()[identifier > tok.colon > type > tok.semicolon];
    on_error<fail>(field, error_handler_function);
    on_success(field, success_handler_function);

    interface.name("interface");
    interface = tok.interface > identifier > tok.l_braces > *(function) > tok.r_braces;
    on_error<fail>(interface, error_handler_function);
    on_success(interface, success_handler_function);

    implement_struct_for_interface.name("implement struct for interface");
    implement_struct_for_interface =
        tok.implement > type > tok.for_ > type > tok.l_braces > *(function) > tok.r_braces;
    on_error<fail>(implement_struct_for_interface, error_handler_function);
    on_success(implement_struct_for_interface, success_handler_function);

    implement_struct.name("implement struct");
    implement_struct = tok.implement > identifier_path > -template_parameters > tok.l_braces >
                       *(function) > tok.r_braces;
    on_error<fail>(implement_struct, error_handler_function);
    on_success(implement_struct, success_handler_function);

    template_parameters.name("template parameters");
    template_parameters = omit[tok.less] > template_parameter % tok.comma > omit[tok.greater];
    on_error<fail>(template_parameters, error_handler_function);
    on_success(template_parameters, success_handler_function);

    template_parameter.name("template parameter");
    template_parameter = as<ast::TemplateParameter>()[identifier > tok.colon > tok.template_];
    on_error<fail>(template_parameter, error_handler_function);
    on_success(template_parameter, success_handler_function);

    function_header.name("function header");
    function_header = annotations >> tok.func > identifier > parameters > -(tok.arrow > type);
    on_error<fail>(function_header, error_handler_function);
    on_success(function_header, success_handler_function);

    function.name("function");
    function = function_header > function_implement;
    on_error<fail>(function, error_handler_function);
    on_success(function, success_handler_function);

    function_implement.name("function implement");
    function_implement = block | tok.semicolon;
    on_error<fail>(function_implement, error_handler_function);
    on_success(function_implement, success_handler_function);

    parameters.name("parameters");
    parameters = omit[tok.l_bracket] > -(parameter % tok.comma) > tok.r_bracket;
    on_error<fail>(parameters, error_handler_function);
    on_success(parameters, success_handler_function);

    parameter.name("paramter");
    parameter = identifier > tok.colon > type;
    on_error<fail>(parameter, error_handler_function);
    on_success(parameter, success_handler_function);

    annotations.name("annotations");
    annotations = *annotation;
    on_error<fail>(annotations, error_handler_function);
    on_success(annotations, success_handler_function);

    annotation.name("annotation");
    annotation = tok.at > identifier >
                 -(omit[tok.l_bracket] > -(expr.string_literal % tok.comma) > tok.r_bracket);
    on_error<fail>(annotation, error_handler_function);
    on_success(annotation, success_handler_function);

    identifier.name("identifier");
    identifier = tok.identifier;
    on_error<fail>(identifier, error_handler_function);
    on_success(identifier, success_handler_function);

    type.name("type");
    type = expr.type;
    on_error<fail>(type, error_handler_function);
    on_success(type, success_handler_function);

    identifier_path.name("identifier path");
    identifier_path = expr.identifier_path;
    on_error<fail>(identifier_path, error_handler_function);
    on_success(identifier_path, success_handler_function);
}

}  // namespace prajna::parser::grammar
