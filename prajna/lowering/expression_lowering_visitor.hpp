
#pragma once

#include <stdexcept>

#include "boost/range/combine.hpp"
#include "boost/variant.hpp"
#include "prajna/assert.hpp"
#include "prajna/ast/ast.hpp"
#include "prajna/exception.hpp"
#include "prajna/helper.hpp"
#include "prajna/ir/ir.hpp"
#include "prajna/logger.hpp"
#include "prajna/lowering/ir_builder.hpp"
#include "prajna/lowering/symbol_table.hpp"
#include "prajna/lowering/template.hpp"

namespace prajna::lowering {

class UndefinedSymbolException : public std::exception {
   public:
    UndefinedSymbolException(ast::IdentifierPath ast_identifier_path) {
        this->identifier_path = ast_identifier_path;
    }

    ast::IdentifierPath identifier_path;
};

class ExpressionLoweringVisitor {
    ExpressionLoweringVisitor() = default;

   public:
    static std::shared_ptr<ExpressionLoweringVisitor> create(std::shared_ptr<IrBuilder> ir_builder,
                                                             std::shared_ptr<Logger> logger) {
        std::shared_ptr<ExpressionLoweringVisitor> self(new ExpressionLoweringVisitor);
        self->logger = logger;
        self->ir_builder = ir_builder;
        return self;
    };

   public:
    bool isIdentifier(ast::IdentifierPath ast_identifier_path) {
        if (ast_identifier_path.root_optional) {
            return false;
        }

        if (ast_identifier_path.identifiers.size() != 1) {
            return false;
        }

        auto identifier_with_template_parameters = ast_identifier_path.identifiers.front();
        return !identifier_with_template_parameters.template_arguments_optional;
    }

    bool isIdentifier(ast::TemplateIdentifier ast_identifier_with_template_parameters) {
        ast::IdentifierPath ast_identifier_path;
        ast_identifier_path.identifiers = {ast_identifier_with_template_parameters};
        return this->isIdentifier(ast_identifier_path);
    }

    ast::Identifier getIdentifier(ast::IdentifierPath ast_identifier_path) {
        return ast_identifier_path.identifiers.front().identifier;
    }

    ast::Identifier getIdentifier(ast::TemplateIdentifier ast_identifier_with_template_parameters) {
        ast::IdentifierPath ast_identifier_path;
        ast_identifier_path.identifiers = {ast_identifier_with_template_parameters};
        return this->getIdentifier(ast_identifier_path);
    }

    std::shared_ptr<ir::Value> apply(ast::Expression ast_expression) {
        PRAJNA_ASSERT(this);
        return (*this)(ast_expression);
    }

    std::shared_ptr<ir::Value> operator()(ast::Blank) {
        PRAJNA_UNREACHABLE;
        return nullptr;
    }

    std::shared_ptr<ir::Value> operator()(ast::CharLiteral ast_char_literal) {
        return ir_builder->create<ir::ConstantChar>(ast_char_literal.value);
    }

    std::shared_ptr<ir::Value> operator()(ast::StringLiteral ast_string_literal) {
        return ir_builder->getString(ast_string_literal.value);
    }

    std::shared_ptr<ir::Value> operator()(ast::IntLiteral ast_int_literal) {
        return ir_builder->getIndexConstant(ast_int_literal.value);
    };

    std::shared_ptr<ir::Value> operator()(ast::IntLiteralPostfix ast_int_literal_postfix) {
        // -1表示位数和是否有符号尚未确定
        // TODO需要进一步处理
        std::string postfix = ast_int_literal_postfix.postfix;
        auto value = ast_int_literal_postfix.int_literal.value;
        if (postfix == "i8") {
            auto ir_type = ir::IntType::create(8, true);
            return ir_builder->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "i16") {
            auto ir_type = ir::IntType::create(16, true);
            return ir_builder->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "i32") {
            auto ir_type = ir::IntType::create(32, true);
            return ir_builder->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "i64") {
            auto ir_type = ir::IntType::create(64, true);
            return ir_builder->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "u8") {
            auto ir_type = ir::IntType::create(8, false);
            return ir_builder->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "u16") {
            auto ir_type = ir::IntType::create(16, false);
            return ir_builder->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "u32") {
            auto ir_type = ir::IntType::create(32, false);
            return ir_builder->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "u64") {
            auto ir_type = ir::IntType::create(64, false);
            return ir_builder->create<ir::ConstantInt>(ir_type, value);
        }

        auto ir_float = createFloatLiteralPostfix(postfix, static_cast<double>(value));
        if (ir_float) {
            return ir_float;
        }

        logger->error(fmt::format("{} is invalid number postfix", postfix),
                      ast_int_literal_postfix.postfix);
        return nullptr;
    };

    std::shared_ptr<ir::Value> createFloatLiteralPostfix(std::string postfix, double value) {
        if (postfix == "f16") {
            auto ir_type = ir::FloatType::create(16);
            return ir_builder->create<ir::ConstantFloat>(ir_type, value);
        }
        if (postfix == "f32") {
            auto ir_type = ir::FloatType::create(32);
            return ir_builder->create<ir::ConstantFloat>(ir_type, value);
        }
        if (postfix == "f64") {
            auto ir_type = ir::FloatType::create(64);
            return ir_builder->create<ir::ConstantFloat>(ir_type, value);
        }

        return nullptr;
    }

    std::shared_ptr<ir::Value> operator()(ast::FloatLiteralPostfix ast_float_literal_postfix) {
        std::string postfix = ast_float_literal_postfix.postfix;
        auto value = ast_float_literal_postfix.float_literal.value;

        auto ir_float = createFloatLiteralPostfix(postfix, value);
        if (ir_float) {
            return ir_float;
        } else {
            logger->error(fmt::format("{} is invalid number postfix", postfix),
                          ast_float_literal_postfix.postfix);
            return nullptr;
        }
    };

    std::shared_ptr<ir::Value> operator()(ast::FloatLiteral ast_float_literal) {
        auto ir_float_type = ir::FloatType::create(32);
        return ir_builder->create<ir::ConstantFloat>(ir_float_type, ast_float_literal.value);
    }

    std::shared_ptr<ir::Value> operator()(ast::BoolLiteral ast_bool_literal) {
        return ir_builder->create<ir::ConstantBool>(ast_bool_literal.value);
    }

    std::shared_ptr<ir::Value> operator()(ast::Identifier ast_identifier) {
        if (!ir_builder->symbol_table->has(ast_identifier)) {
            logger->error(fmt::format("{} not found", ast_identifier), ast_identifier);
        }

        auto symbol = ir_builder->symbol_table->get(ast_identifier);
        if (!symbolIs<ir::Value>(symbol)) {
            PRAJNA_UNREACHABLE;
            logger->error(fmt::format("{} is not a valid value"), ast_identifier);
        }

        return symbolGet<ir::Value>(symbol);
    }

    std::shared_ptr<ir::Value> operator()(ast::Expression ast_expresion) {
        auto ir_lhs = applyOperand(ast_expresion.first);
        ir_lhs->source_location = ast_expresion;
        for (auto optoken_and_rhs : ast_expresion.rest) {
            ir_lhs = applyBinaryOperation(ir_lhs, optoken_and_rhs);
            ir_lhs->source_location = optoken_and_rhs;
        }
        return ir_lhs;
    }

    std::shared_ptr<ir::Value> applyBinaryOperationAccessMember(
        std::shared_ptr<ir::Value> ir_lhs, ast::BinaryOperation ast_binary_operation) {
        if (ast_binary_operation.operand.type() != typeid(ast::IdentifierPath)) {
            PRAJNA_UNREACHABLE;
            logger->error("access operand must be a identifier path", ast_binary_operation);
        }

        auto identifier_path = boost::get<ast::IdentifierPath>(ast_binary_operation.operand);
        PRAJNA_ASSERT(identifier_path.identifiers.size() == 1);
        std::string member_name = identifier_path.identifiers.front().identifier;

        // 模板函数
        if (identifier_path.identifiers.front().template_arguments_optional) {
            ir_builder->instantiateTypeImplements(ir_lhs->type);
            if (!ir_lhs->type->template_any_dict.count(member_name)) {
                logger->error("invalid template", ast_binary_operation.operand);
            }
            auto lowering_member_function_template = std::any_cast<std::shared_ptr<Template>>(
                ir_lhs->type->template_any_dict[member_name]);

            auto symbol_template_arguments = this->applyTemplateArguments(
                *identifier_path.identifiers.front().template_arguments_optional);

            auto ir_member_function = cast<ir::Function>(
                symbolGet<ir::Value>(lowering_member_function_template->instantiate(
                    symbol_template_arguments, ir_builder->module)));
            PRAJNA_ASSERT(ir_member_function);

            auto ir_variable_liked = ir_builder->variableLikedNormalize(ir_lhs);
            auto ir_this_pointer =
                ir_builder->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
            return ir::MemberFunctionWithThisPointer::create(ir_this_pointer, ir_member_function);
        }

        if (!ir_lhs->type) {
            this->logger->error("invalid object type to access member", ir_lhs->source_location);
        }

        if (auto ir_member = ir_builder->accessMember(ir_lhs, member_name)) {
            return ir_member;
        }

        // 语法糖, ptr类型会寻找raw_ptr的成员函数
        if (ir_builder->isPtrType(ir_lhs->type)) {
            auto ir_raw_ptr = ir_builder->accessField(ir_lhs, "raw_ptr");
            auto ir_object = ir_builder->create<ir::DeferencePointer>(ir_raw_ptr);
            if (auto ir_member =
                    this->applyBinaryOperationAccessMember(ir_object, ast_binary_operation)) {
                return ir_member;
            }
        }

        logger->error(fmt::format("{} is not a member of {}", member_name, ir_lhs->type->fullname),
                      ast_binary_operation.operand);
        return nullptr;
    }

    std::shared_ptr<ir::Value> applyBinaryOperationIndexArray(
        std::shared_ptr<ir::Value> ir_object, ast::BinaryOperation ast_binary_operation) {
        auto ir_variable_liked = ir_builder->variableLikedNormalize(ir_object);
        auto ir_arguments =
            *cast<ir::ValueCollection>(this->applyOperand(ast_binary_operation.operand));
        if (ir_arguments.empty()) {
            logger->error("index should have one argument at least", ast_binary_operation.operand);
        }
        auto ir_index = ir_arguments.front();
        if (ir_index->type != ir_builder->getIndexType()) {
            logger->error(
                fmt::format("the index type must be i64, but it's {}", ir_index->type->fullname),
                ast_binary_operation.operand);
        }
        // 之所以没包装成模板函数, 是因为需要包装成属性过于复杂了. 这样反而比较简单
        if (is<ir::ArrayType>(ir_object->type)) {
            if (ir_arguments.size() > 1) {
                logger->error("too many index arguments", ast_binary_operation.operand);
            }
            return ir_builder->create<ir::IndexArray>(ir_variable_liked, ir_index);
        }
        if (is<ir::PointerType>(ir_object->type)) {
            if (ir_arguments.size() > 1) {
                logger->error("too many index arguments", ast_binary_operation.operand);
            }
            return ir_builder->create<ir::IndexPointer>(ir_variable_liked, ir_index);
        }

        if (auto ir_linear_index_property = ir_builder->getLinearIndexProperty(ir_object->type)) {
            auto ir_this_pointer =
                ir_builder->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
            auto ir_access_property =
                ir_builder->create<ir::AccessProperty>(ir_this_pointer, ir_linear_index_property);
            ir_access_property->arguments({ir_index});
            return ir_access_property;
        }

        if (auto ir_array_index_property = ir_builder->getArrayIndexProperty(ir_object->type)) {
            auto ast_arguments = boost::get<ast::Expressions>(ast_binary_operation.operand);
            auto ir_array_argument = this->applyArray(ast_arguments);

            auto ir_this_pointer =
                ir_builder->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
            auto ir_access_property =
                ir_builder->create<ir::AccessProperty>(ir_this_pointer, ir_array_index_property);
            ir_access_property->arguments({ir_array_argument});
            return ir_access_property;
        }

        logger->error("the object has not a \"[\" property", ast_binary_operation);

        return nullptr;
    }

    std::shared_ptr<ir::Value> applyBinaryOperationCall(std::shared_ptr<ir::Value> ir_lhs,
                                                        ast::BinaryOperation ast_binary_operaton) {
        auto ir_arguments = *cast<ir::ValueCollection>(applyOperand(ast_binary_operaton.operand));
        PRAJNA_ASSERT(ast_binary_operaton.operand.type() == typeid(ast::Expressions));
        auto ast_expressions = boost::get<ast::Expressions>(ast_binary_operaton.operand);
        if (auto ir_member_function = cast<ir::MemberFunctionWithThisPointer>(ir_lhs)) {
            auto ir_function_type = ir_member_function->function_prototype->function_type;
            if (ir_arguments.size() + 1 != ir_function_type->parameter_types.size()) {
                logger->error(
                    fmt::format(
                        "the arguments size is not matched, require {} argument, but give {}",
                        ir_function_type->parameter_types.size() - 1, ir_arguments.size()),
                    ast_expressions);
            }
            for (auto [ir_argument, ir_parameter_type, ast_expression] :
                 boost::combine(ir_arguments,
                                boost::make_iterator_range(
                                    std::next(ir_function_type->parameter_types.begin()),
                                    ir_function_type->parameter_types.end()),
                                ast_expressions)) {
                if (ir_argument->type != ir_parameter_type) {
                    logger->error("the argument type is not matched", ast_expression);
                }
            }

            ir_arguments.insert(ir_arguments.begin(), ir_member_function->this_pointer);
            return ir_builder->create<ir::Call>(ir_member_function->function_prototype,
                                                ir_arguments);
        }

        if (ir_lhs->isFunction()) {
            auto ir_function_type = ir_lhs->getFunctionType();
            if (ir_arguments.size() != ir_function_type->parameter_types.size()) {
                logger->error(
                    fmt::format(
                        "the arguments size is not matched, require {} argument, but give {}",
                        ir_function_type->parameter_types.size(), ir_arguments.size()),
                    ast_expressions);
            }
            for (auto [ir_argument, ir_parameter_type, ast_experssion] :
                 boost::combine(ir_arguments, ir_function_type->parameter_types, ast_expressions)) {
                if (ir_argument->type != ir_parameter_type) {
                    logger->error("the argument type is not matched", ast_expressions);
                }
            }
            return ir_builder->create<ir::Call>(ir_lhs, ir_arguments);
        }

        if (auto ir_access_property = cast<ir::AccessProperty>(ir_lhs)) {
            // getter函数必须存在
            auto ir_getter_function_type =
                ir_access_property->property->get_function->function_type;
            if (ir_arguments.size() != ir_getter_function_type->parameter_types.size() - 1) {
                logger->error(
                    fmt::format("the property arguments size is not matched, require {} argument, "
                                "but give {}",
                                ir_getter_function_type->parameter_types.size() - 1,
                                ir_arguments.size()),
                    ast_expressions);
            }

            for (auto [ir_argument, ir_parameter_type, ast_expression] :
                 boost::combine(ir_arguments,
                                boost::make_iterator_range(
                                    std::next(ir_getter_function_type->parameter_types.begin()),
                                    ir_getter_function_type->parameter_types.end()),
                                ast_expressions)) {
                if (ir_argument->type != ir_parameter_type) {
                    logger->error("the argument type is not matched", ast_expression);
                }
            }

            // 移除, 在末尾插入, 应为参数应该在属性访问的前面
            ir_access_property->parent_block->values.remove(ir_access_property);
            ir_builder->currentBlock()->values.push_back(ir_access_property);
            ir_access_property->arguments(ir_arguments);
            return ir_access_property;
        }

        logger->error("not a valid callable", ast_binary_operaton);
        return nullptr;
    }

    std::shared_ptr<ir::Value> applyBinaryOperation(std::shared_ptr<ir::Value> ir_lhs,
                                                    ast::BinaryOperation ast_binary_operation) {
        if (ast_binary_operation.operator_ == ast::Operator(".")) {
            return this->applyBinaryOperationAccessMember(ir_lhs, ast_binary_operation);
        }
        if (ast_binary_operation.operator_ == ast::Operator("(")) {
            return this->applyBinaryOperationCall(ir_lhs, ast_binary_operation);
        }
        if (ast_binary_operation.operator_ == ast::Operator("[")) {
            return this->applyBinaryOperationIndexArray(ir_lhs, ast_binary_operation);
        } else {
            // 将其余的运算都转换为函数调用
            return this->convertBinaryOperationToCallFunction(ir_lhs, ast_binary_operation);
        }

        PRAJNA_UNREACHABLE;
        return nullptr;
    }

    std::shared_ptr<ir::Value> operator()(ast::Expressions ast_expressions) {
        // 仅用于辅助, 故不加入IR中
        auto ir_values = std::make_shared<ir::ValueCollection>();
        for (auto ast_expresion : ast_expressions) {
            auto ir_value = (*this)(ast_expresion);
            ir_values->push_back(ir_value);
        }
        return ir_values;
    }

    std::shared_ptr<ir::Value> applyUnaryOperatorDereferencePointer(
        std::shared_ptr<ir::Value> ir_operand) {
        return ir_builder->create<ir::DeferencePointer>(ir_operand);
    }

    std::shared_ptr<ir::Value> operator()(ast::Unary ast_unary) {
        auto ir_operand = this->applyOperand(ast_unary.operand);
        if (ast_unary.operator_ == ast::Operator("*")) {
            return this->applyUnaryOperatorDereferencePointer(ir_operand);
        }
        if (ast_unary.operator_ == ast::Operator("&")) {
            auto ir_variable_liked = cast<ir::VariableLiked>(ir_operand);
            if (ir_variable_liked == nullptr) {
                logger->error("only could get the address of a VariableLiked", ast_unary);
            }
            return ir_builder->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        }

        auto ir_variable_liked = ir_builder->variableLikedNormalize(ir_operand);
        auto unary_operator_name = ast_unary.operator_.string_token;
        auto ir_function =
            ir_builder->getUnaryOperator(ir_variable_liked->type, unary_operator_name);
        if (not ir_function) {
            logger->error("unary operator not found", ast_unary.operator_);
        }

        auto ir_this_pointer = ir_builder->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        std::list<std::shared_ptr<ir::Value>> ir_arguemnts;
        ir_arguemnts.push_back(ir_this_pointer);
        return ir_builder->create<ir::Call>(ir_function, ir_arguemnts);
    }

    std::shared_ptr<ir::Type> applyType(ast::Type ast_type) {
        auto symbol_type = this->applyIdentifierPath(ast_type);
        auto ir_type = symbolGet<ir::Type>(symbol_type);
        if (!ir_type) {
            logger->error("the symbol is not a type", ast_type);
        }
        return ir_type;
    }

    std::shared_ptr<TemplateStruct> createDynamicTemplate() {
        auto template_dynamic = Template::create();
        template_dynamic->generator = [symbol_table = this->ir_builder->symbol_table,
                                       logger = this->logger,
                                       this](std::list<Symbol> symbol_template_arguments,
                                             std::shared_ptr<ir::Module> ir_module) -> Symbol {
            // TODO
            PRAJNA_ASSERT(symbol_template_arguments.size() == 1);

            auto ir_interface_prototype =
                symbolGet<ir::InterfacePrototype>(symbol_template_arguments.front());
            PRAJNA_ASSERT(ir_interface_prototype);
            return ir_interface_prototype->dynamic_type;
        };

        auto template_struct_dynamic = TemplateStruct::create();
        template_struct_dynamic->template_struct_impl = template_dynamic;
        return template_struct_dynamic;
    }

    std::list<Symbol> applyTemplateArguments(ast::TemplateArguments ast_template_arguments) {
        std::list<Symbol> symbol_template_arguments;
        for (auto ast_template_argument : ast_template_arguments) {
            auto symbol_template_argument = boost::apply_visitor(
                overloaded{[=](ast::Blank) -> Symbol {
                               PRAJNA_UNREACHABLE;
                               return nullptr;
                           },
                           [=](ast::IdentifierPath ast_identifier_path) -> Symbol {
                               return this->applyIdentifierPath(ast_identifier_path);
                           },
                           [=](ast::IntLiteral ast_int_literal) -> Symbol {
                               return ir::ConstantInt::create(ir_builder->getIndexType(),
                                                              ast_int_literal.value);
                           }},
                ast_template_argument);
            symbol_template_arguments.push_back(symbol_template_argument);
        }

        return symbol_template_arguments;
    }

    Symbol applyIdentifierPath(ast::IdentifierPath ast_identifier_path) {
        Symbol symbol;
        if (ast_identifier_path.root_optional) {
            symbol = ir_builder->symbol_table->rootSymbolTable();
        } else {
            symbol = ir_builder->symbol_table;
        }
        PRAJNA_ASSERT(symbol.which() != 0);

        for (auto iter_ast_identifier = ast_identifier_path.identifiers.begin();
             iter_ast_identifier != ast_identifier_path.identifiers.end(); ++iter_ast_identifier) {
            std::string flag = iter_ast_identifier->identifier;

            symbol = boost::apply_visitor(
                overloaded{
                    [=](auto x) -> Symbol {
                        PRAJNA_UNREACHABLE;
                        return nullptr;
                    },
                    [=](std::shared_ptr<ir::Value> ir_value) -> Symbol {
                        logger->error("not a valid scope access", *iter_ast_identifier);
                        return nullptr;
                    },
                    [=](std::shared_ptr<ir::Type> ir_type) -> Symbol {
                        auto static_function_identifier = iter_ast_identifier->identifier;
                        if (iter_ast_identifier->template_arguments_optional) {
                            auto lowering_member_function_template =
                                std::any_cast<std::shared_ptr<Template>>(
                                    ir_type->template_any_dict[static_function_identifier]);
                            auto symbol_template_argumen_list = this->applyTemplateArguments(
                                *iter_ast_identifier->template_arguments_optional);
                            auto ir_function = cast<ir::Function>(
                                symbolGet<ir::Value>(lowering_member_function_template->instantiate(
                                    symbol_template_argumen_list, ir_builder->module)));
                            PRAJNA_ASSERT(ir_function);
                            return ir_function;
                        }

                        auto ir_static_fun =
                            ir_builder->GetImplementFunction(ir_type, static_function_identifier);
                        if (ir_static_fun == nullptr) {
                            logger->error(
                                fmt::format("the static function {} is not exit in type {}",
                                            static_function_identifier, ir_type->fullname),
                                static_function_identifier);
                        }

                        return ir_static_fun;
                    },
                    [=](std::shared_ptr<TemplateStruct> template_strcut) -> Symbol {
                        PRAJNA_UNREACHABLE;
                        return nullptr;
                    },
                    [=](std::shared_ptr<SymbolTable> symbol_table) -> Symbol {
                        auto symbol = symbol_table->get(iter_ast_identifier->identifier);
                        if (symbol.which() == 0) {
                            logger->error("the symbol is not found",
                                          iter_ast_identifier->identifier);
                        }

                        if (!iter_ast_identifier->template_arguments_optional) {
                            return symbol;
                        } else {
                            try {
                                auto symbol_template_arguments = this->applyTemplateArguments(
                                    *iter_ast_identifier->template_arguments_optional);

                                if (auto template_struct = symbolGet<TemplateStruct>(symbol)) {
                                    // 如果获取到nullptr则说明实例化正在进行中,
                                    // 使用instantiating_type来获取相应类型
                                    if (auto ir_type = template_struct->instantiate(
                                            symbol_template_arguments, ir_builder->module,
                                            ir_builder->instantiating_type_stack.size() ||
                                                ir_builder->interface_prototype_processing)) {
                                        return ir_type;
                                    } else {
                                        PRAJNA_ASSERT(ir_builder->instantiating_type_stack.size());
                                        return ir_builder->instantiating_type_stack.top();
                                    }
                                }

                                if (auto tempate_ = symbolGet<Template>(symbol)) {
                                    return tempate_->instantiate(symbol_template_arguments,
                                                                 ir_builder->module);
                                }

                                logger->error("not a template", iter_ast_identifier->identifier);

                                return nullptr;
                            } catch (CompileError compile_error) {
                                logger->note(*iter_ast_identifier);
                                throw compile_error;
                            }
                        }
                    }},
                symbol);
        }

        return symbol;
    }

    std::shared_ptr<ir::Value> operator()(ast::IdentifierPath ast_identifier_path) {
        auto symbol = this->applyIdentifierPath(ast_identifier_path);
        return boost::apply_visitor(
            overloaded{
                [](std::shared_ptr<ir::Value> ir_value) -> std::shared_ptr<ir::Value> {
                    return ir_value;
                },
                [=](std::shared_ptr<ir::Type> ir_type) -> std::shared_ptr<ir::Value> {
                    /// TODO 需要重构, var t = Tensor<i64, 3>;也会被认为正确, 存在误导,
                    /// 后面看怎么处理
                    if (ir_type->constructor == nullptr) {
                        logger->error(fmt::format("{} has no constructor", ir_type->fullname),
                                      ast_identifier_path);
                    }
                    return ir_type->constructor;
                },
                // template parameter could be a ConstantInt.
                [ir_builder = this->ir_builder](std::shared_ptr<ir::ConstantInt> ir_constant_int)
                    -> std::shared_ptr<ir::Value> {
                    // 需要在block里插入, 这才符合后面IR转换生成的规则
                    // TODO 可以挪到transform里做这个事
                    PRAJNA_ASSERT(ir_constant_int->type == ir_builder->getIndexType());
                    return ir_builder->getIndexConstant(ir_constant_int->value);
                },
                [=](auto x) {
                    logger->error(fmt::format("use invalid symbol as a value"),
                                  ast_identifier_path);
                    return nullptr;
                }},
            symbol);
    }

    std::shared_ptr<ir::Value> applyArray(std::list<ast::Expression> ast_array_values) {
        // 获取数组类型
        PRAJNA_ASSERT(ast_array_values.size() > 0);
        auto ir_first_value = this->applyOperand(ast_array_values.front());
        auto ir_value_type = ir_first_value->type;
        std::list<Symbol> symbol_template_arguments;
        symbol_template_arguments.push_back(ir_value_type);
        symbol_template_arguments.push_back(ir_builder->getIndexConstant(ast_array_values.size()));

        auto symbol_array = ir_builder->symbol_table->get("Array");
        PRAJNA_VERIFY(symbol_array.type() == typeid(std::shared_ptr<TemplateStruct>),
                      "system libs is bad");
        auto array_template = symbolGet<TemplateStruct>(symbol_array);
        auto ir_array_type =
            array_template->instantiate(symbol_template_arguments, ir_builder->module);

        auto ir_array_tmp = ir_builder->create<ir::LocalVariable>(ir_array_type);
        auto ir_index_property = ir_builder->getLinearIndexProperty(ir_array_type);
        PRAJNA_VERIFY(ir_index_property, "Array index property is missing");
        size_t i = 0;
        for (auto ast_array_value : ast_array_values) {
            auto ir_value = this->applyOperand(ast_array_value);
            if (ir_value->type != ir_value_type) {
                logger->error("the array element type are not the same", ast_array_value);
            }
            auto ir_index = ir_builder->getIndexConstant(i);
            auto ir_array_tmp_this_pointer =
                ir_builder->create<ir::GetAddressOfVariableLiked>(ir_array_tmp);
            auto ir_access_property = ir_builder->create<ir::AccessProperty>(
                ir_array_tmp_this_pointer, ir_index_property);
            ir_access_property->arguments({ir_index});
            ir_builder->create<ir::WriteProperty>(ir_value, ir_access_property);
            ++i;
        }

        return ir_array_tmp;
    }

    std::shared_ptr<ir::Value> operator()(ast::Array ast_array) {
        return applyArray(ast_array.values);
    }

    std::shared_ptr<ir::Value> operator()(ast::KernelFunctionCall ast_kernel_function_call) {
        auto ir_function = (*this)(ast_kernel_function_call.kernel_function);
        if (ast_kernel_function_call.operation) {
            // TODO 核函数必须有判断@kernel
            auto ir_function_type = ir_function->getFunctionType();
            if (ir_function_type == nullptr) {
                logger->error("not callable", ast_kernel_function_call.kernel_function);
            }
            auto ir_grid_shape = (*this)(ast_kernel_function_call.operation->grid_shape);
            auto ir_block_shape = (*this)(ast_kernel_function_call.operation->block_shape);
            auto ir_arguments =
                *cast<ir::ValueCollection>((*this)(ast_kernel_function_call.operation->arguments));
            // TODO arguments
            auto ir_shape3_type = ir_builder->getShape3Type();
            if (ir_grid_shape->type != ir_shape3_type) {
                logger->error("the grid dim type must be i64[3]",
                              ast_kernel_function_call.operation->grid_shape);
            }
            if (ir_block_shape->type != ir_shape3_type) {
                logger->error("the block dim type must be i64[3]",
                              ast_kernel_function_call.operation->block_shape);
            }

            if (ir_arguments.size() != ir_function_type->parameter_types.size()) {
                logger->error(
                    fmt::format("the arguments size is not matched, require {} "
                                "argument, but give {}",
                                ir_function_type->parameter_types.size(), ir_arguments.size()),
                    ast_kernel_function_call.operation->arguments);
            }

            for (auto [ir_argument, ir_parameter_type, ast_argument] :
                 boost::combine(ir_arguments, ir_function_type->parameter_types,
                                ast_kernel_function_call.operation->arguments)) {
                if (ir_argument->type != ir_parameter_type) {
                    logger->error("the argument type is not matched", ast_argument);
                }
            }
            return ir_builder->create<ir::KernelFunctionCall>(ir_function, ir_grid_shape,
                                                              ir_block_shape, ir_arguments);

        } else {
            return ir_function;
        }

        PRAJNA_UNREACHABLE;
        return nullptr;
    }

    template <typename Ast_>
    std::shared_ptr<ir::Value> operator()(Ast_ ast_operand) {
        PRAJNA_ASSERT(false, typeid(ast_operand).name());
        return nullptr;
    }

    std::shared_ptr<ir::Value> applyOperand(ast::Operand ast_operand) {
        PRAJNA_ASSERT(this);
        return boost::apply_visitor(*this, ast_operand);
    }

    std::shared_ptr<ir::Value> convertBinaryOperationToCallFunction(
        std::shared_ptr<ir::Value> ir_lhs, ast::BinaryOperation ast_binary_operation) {
        // 会把binary operator转换为成员函数的调用
        auto ir_variable_liked = ir_builder->variableLikedNormalize(ir_lhs);
        auto binary_operator_name = ast_binary_operation.operator_.string_token;
        auto ir_function =
            ir_builder->getBinaryOperator(ir_variable_liked->type, binary_operator_name);
        if (not ir_function) {
            logger->error(
                fmt::format("{} operator not found", ast_binary_operation.operator_.string_token),
                ast_binary_operation.operator_);
        }
        auto ir_this_pointer = ir_builder->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        std::list<std::shared_ptr<ir::Value>> ir_arguemnts(2);
        ir_arguemnts.front() = ir_this_pointer;
        ir_arguemnts.back() = this->applyOperand(ast_binary_operation.operand);
        if (ir_arguemnts.back()->type != ir_function->function_type->parameter_types.back()) {
            logger->error(fmt::format("the types {}, {} are not matched",
                                      ir_function->function_type->parameter_types.back()->fullname,
                                      ir_arguemnts.back()->type->fullname),
                          ast_binary_operation.operand);
        }
        return ir_builder->create<ir::Call>(ir_function, ir_arguemnts);
    }

   private:
    std::shared_ptr<IrBuilder> ir_builder;
    std::shared_ptr<Logger> logger;
};

}  // namespace prajna::lowering
