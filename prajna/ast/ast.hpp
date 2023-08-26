#pragma once

#include <list>
#include <optional>

#include "boost/fusion/include/adapt_struct.hpp"
#include "boost/fusion/include/io.hpp"
#include "boost/optional.hpp"
#include "boost/variant/recursive_variant.hpp"
#include "prajna/ast/source_position.hpp"
#include "prajna/config.hpp"

namespace prajna::ast {

struct SourceLocation {
    SourcePosition first_position;
    SourcePosition last_position;
};

struct Blank : SourceLocation {};

struct Unary;
struct PostfixUnary;
struct Expression;
struct Expressions;
struct KernelFunctionCall;
struct PrefixCall;
struct Array;
struct Template;
struct TemplateStatement;
struct Module;
struct Closure;
struct SpecialStatement;

struct Operator : SourceLocation {
    Operator() = default;

    explicit Operator(const std::string& chars_) : string_token(chars_) {}

    std::string string_token;
};

inline bool operator==(const Operator& lhs, const Operator& rhs) {
    return (lhs.string_token == rhs.string_token);
}

inline bool operator!=(const Operator& lhs, const Operator& rhs) {
    return !(lhs.string_token == rhs.string_token);
}

struct Identifier : public std::string, public SourceLocation {
    Identifier() : std::string() {}
    Identifier(const Identifier& rhs) : std::string(rhs) {
        first_position = rhs.first_position;
        last_position = rhs.last_position;
    }

    Identifier(const std::string& s) : std::string(s) {}

    Identifier& operator=(const Identifier& other) {
        std::string::operator=(other);
        first_position = other.first_position;
        last_position = other.last_position;
        return *this;
    }

    Identifier& operator=(const std::string& other) {
        std::string::operator=(other);
        return *this;
    }
};

struct BoolLiteral : SourceLocation {
    bool value;
};

struct IntLiteral : SourceLocation {
    int64_t value;
};

struct IntLiteralPostfix : SourceLocation {
    IntLiteral int_literal;
    Identifier postfix;
};

struct FloatLiteral : SourceLocation {
    double value;
};

struct FloatLiteralPostfix : SourceLocation {
    FloatLiteral float_literal;
    Identifier postfix;
};

struct CharLiteral : SourceLocation {
    char value;
};

struct StringLiteral : SourceLocation {
    std::string value;
};

struct Annotation : SourceLocation {
    Identifier name;
    std::list<StringLiteral> values;
};

struct AnnotationDict : SourceLocation, std::list<Annotation> {};

struct IdentifierPath;

typedef IdentifierPath Type;

using TemplateArgument =
    boost::variant<Blank, boost::recursive_wrapper<IdentifierPath>, IntLiteral>;

struct TemplateArguments : SourceLocation, std::list<TemplateArgument> {};

struct TemplateIdentifier : SourceLocation {
    Identifier identifier;
    // @note boost::optional是不可以省略的, 会有未知错误,
    // 可能和boost::recursive_wrapper混合使用有关, 目前就不省略了
    boost::optional<TemplateArguments> template_arguments_optional;
};

struct IdentifierPath : SourceLocation {
    boost::optional<Operator> root_optional;
    std::list<TemplateIdentifier> identifiers;
};

struct Use : SourceLocation {
    IdentifierPath identifier_path;
    boost::optional<Operator> star_match_optional;
    boost::optional<Identifier> as_optional;
};

typedef boost::variant<Blank, CharLiteral, StringLiteral, BoolLiteral, IntLiteral, FloatLiteral,
                       Identifier, IdentifierPath, IntLiteralPostfix, FloatLiteralPostfix,
                       boost::recursive_wrapper<Closure>, boost::recursive_wrapper<Unary>,
                       boost::recursive_wrapper<PostfixUnary>, boost::recursive_wrapper<Expression>,
                       boost::recursive_wrapper<Expressions>, boost::recursive_wrapper<Array>,
                       boost::recursive_wrapper<KernelFunctionCall>>
    Operand;

struct Unary : SourceLocation {
    Operator operator_;
    Operand operand;
};

struct PostfixUnary : SourceLocation {
    Operand operand;
    std::list<Operator> operators;
};

struct BinaryOperation : SourceLocation {
    Operator operator_;
    Operand operand;
};

struct Expression : SourceLocation {
    Operand first;
    std::list<BinaryOperation> rest;
};

struct Expressions : SourceLocation, std::list<Expression> {};

struct Array : SourceLocation {
    std::list<Expression> values;
};

struct VariableDeclaration : SourceLocation {
    AnnotationDict annotation_dict;
    Identifier name;
    boost::optional<Type> type_optional;
    boost::optional<Expression> initialize_optional;
};

struct Assignment : SourceLocation {
    Expression left;
    Expression right;
};

struct Break : SourceLocation {};

struct Continue : SourceLocation {};

struct If;
struct While;
struct For;
struct Return;
struct Struct;
struct InterfacePrototype;
struct ImplementType;
struct ImplementInterfaceForType;
struct Class;
struct FunctionHeader;
struct Function;
struct Operator;
struct Block;

struct Pragma : SourceLocation {
    Identifier name;
    std::list<StringLiteral> values;
};

template <typename _T>
struct Annotated {
    AnnotationDict annoations;
    _T statement;
};

struct Statements;

typedef boost::variant<
    Blank, boost::recursive_wrapper<Statements>, boost::recursive_wrapper<Module>,
    boost::recursive_wrapper<Block>, Use, VariableDeclaration, Assignment, Expression,
    boost::recursive_wrapper<If>, boost::recursive_wrapper<While>, boost::recursive_wrapper<For>,
    Break, Continue, boost::recursive_wrapper<Function>, boost::recursive_wrapper<Return>,
    boost::recursive_wrapper<Struct>, boost::recursive_wrapper<InterfacePrototype>,
    boost::recursive_wrapper<ImplementType>, boost::recursive_wrapper<ImplementInterfaceForType>,
    boost::recursive_wrapper<Template>, boost::recursive_wrapper<TemplateStatement>,
    boost::recursive_wrapper<SpecialStatement>, Pragma>
    Statement;

// @note需要声明为class, 因为之前才能在其他模块使用前置声明.
struct Statements : SourceLocation, public std::list<Statement> {};

struct Module : SourceLocation {
    IdentifierPath name;
    Statements statements;
};

struct Block : SourceLocation {
    Statements statements;
};

struct If : SourceLocation {
    Expression condition;
    Block then;
    boost::optional<Block> else_optional;
};

struct While : SourceLocation {
    Expression condition;
    Block body;
};

struct For : SourceLocation {
    AnnotationDict annotation_dict;
    Identifier index;
    Expression first;
    Expression last;
    Block body;
};

struct Return : SourceLocation {
    boost::optional<Expression> expr_optional;
};

struct Parameter : SourceLocation {
    Identifier name;
    Type type;
};

struct Parameters : SourceLocation, std::list<Parameter> {};

struct FunctionHeader : SourceLocation {
    AnnotationDict annotation_dict;
    Identifier name;
    Parameters parameters;
    boost::optional<Type> return_type_optional;
};

struct Function : SourceLocation {
    FunctionHeader declaration;
    boost::optional<Block> body_optional;
};

struct Closure : SourceLocation {
    Parameters parameters;
    boost::optional<Type> return_type_optional;
    Block body;
};

struct Field : SourceLocation {
    Identifier name;
    Type type;
};

struct TemplateParameter : SourceLocation {
    Identifier name;
    boost::optional<IdentifierPath> concept_optional;
};

struct TemplateParameters : SourceLocation, std::list<TemplateParameter> {};

struct Struct : SourceLocation {
    Identifier name;
    std::list<Field> fields;
};

struct InterfacePrototype : SourceLocation {
    AnnotationDict annotation_dict;
    Identifier name;
    // 函数声明也采用Function, 但其没有实现
    std::list<Function> functions;
};

struct ImplementType : SourceLocation {
    Type type;
    Statements statements;
};

struct ImplementInterfaceForType : SourceLocation {
    AnnotationDict annotation_dict;
    IdentifierPath interface;
    Type type;
    std::list<Function> functions;
};

struct Template : SourceLocation {
    Identifier name;
    TemplateParameters template_parameters;
    Statements statements;
};

typedef boost::variant<Function, Struct, InterfacePrototype, ImplementType,
                       ImplementInterfaceForType>
    TemplateAbleStatement;

struct TemplateStatement : SourceLocation {
    TemplateParameters template_parameters;
    TemplateAbleStatement statement;
};

struct SpecialStatement : SourceLocation {
    TemplateArguments template_arguments;
    boost::recursive_wrapper<Statement> statement;
};

struct KernelFunctionCallOperation {
    Expression grid_shape;
    Expression block_shape;
    Expressions arguments;
};

struct KernelFunctionCall : SourceLocation {
    Expression kernel_function;
    boost::optional<KernelFunctionCallOperation> operation;
};

}  // namespace prajna::ast

BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Module, name, statements)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Block, statements)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Unary, operator_, operand)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::PostfixUnary, operand, operators)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::BinaryOperation, operator_, operand)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Assignment, left, right)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Expression, first, rest)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::VariableDeclaration, annotation_dict, name, type_optional,
                          initialize_optional)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::If, condition, then, else_optional)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::While, condition, body)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::For, annotation_dict, index, first, last, body)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Return, expr_optional)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Field, name, type)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::TemplateIdentifier, identifier, template_arguments_optional)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Struct, name, fields)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Parameter, name, type)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::FunctionHeader, annotation_dict, name, parameters,
                          return_type_optional)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Pragma, name, values)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Annotation, name, values)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Function, declaration, body_optional)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Closure, parameters, return_type_optional, body)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Template, name, template_parameters, statements)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::TemplateStatement, template_parameters, statement)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::SpecialStatement, template_arguments, statement)

BOOST_FUSION_ADAPT_STRUCT(prajna::ast::InterfacePrototype, annotation_dict, name, functions)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::ImplementType, type, statements)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::ImplementInterfaceForType, annotation_dict, interface, type,
                          functions)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::IdentifierPath, root_optional, identifiers)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Use, identifier_path, star_match_optional, as_optional)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::IntLiteralPostfix, int_literal, postfix)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::FloatLiteralPostfix, float_literal, postfix)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::KernelFunctionCallOperation, grid_shape, block_shape,
                          arguments)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::KernelFunctionCall, kernel_function, operation)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::Array, values)
BOOST_FUSION_ADAPT_STRUCT(prajna::ast::TemplateParameter, name, concept_optional)

BOOST_FUSION_ADAPT_TPL_STRUCT((_T), (prajna::ast::Annotated)(_T), annotation_dict, statement)
