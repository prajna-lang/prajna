
#pragma once

#include <stdexcept>

#include "boost/variant.hpp"
#include "prajna/assert.hpp"
#include "prajna/ast/ast.hpp"
#include "prajna/exception.hpp"
#include "prajna/helper.hpp"
#include "prajna/ir/ir.hpp"
#include "prajna/logger.hpp"
#include "prajna/lowering/ir_utility.hpp"
#include "prajna/lowering/symbol_table.hpp"
#include "prajna/lowering/template.hpp"

namespace prajna::lowering {

class UndefinedSymbolException : public std::exception {
   public:
    UndefinedSymbolException(ast::IdentifiersResolution ast_identifiers_resolution) {
        this->identifiers_resolution = ast_identifiers_resolution;
    }

    ast::IdentifiersResolution identifiers_resolution;
};

class ExpressionLoweringVisitor {
    ExpressionLoweringVisitor() = default;

   public:
    static std::shared_ptr<ExpressionLoweringVisitor> create(std::shared_ptr<IrUtility> ir_utility,
                                                             std::shared_ptr<Logger> logger) {
        std::shared_ptr<ExpressionLoweringVisitor> self(new ExpressionLoweringVisitor);
        self->logger = logger;
        self->ir_utility = ir_utility;
        return self;
    };

   public:
    bool isIdentifier(ast::IdentifiersResolution ast_identifiers_resolution) {
        if (ast_identifiers_resolution.is_root) {
            return false;
        }

        if (ast_identifiers_resolution.identifiers.size() != 1) {
            return false;
        }

        auto identifier_with_template_parameters = ast_identifiers_resolution.identifiers.front();
        return !identifier_with_template_parameters.template_arguments;
    }

    bool isIdentifier(
        ast::IdentifierWithTemplateArguments ast_identifier_with_template_parameters) {
        ast::IdentifiersResolution ast_identifiers_resolution;
        ast_identifiers_resolution.identifiers = {ast_identifier_with_template_parameters};
        return this->isIdentifier(ast_identifiers_resolution);
    }

    ast::Identifier getIdentifier(ast::IdentifiersResolution ast_identifiers_resolution) {
        return ast_identifiers_resolution.identifiers.front().identifier;
    }

    ast::Identifier getIdentifier(
        ast::IdentifierWithTemplateArguments ast_identifier_with_template_parameters) {
        ast::IdentifiersResolution ast_identifiers_resolution;
        ast_identifiers_resolution.identifiers = {ast_identifier_with_template_parameters};
        return this->getIdentifier(ast_identifiers_resolution);
    }

    std::shared_ptr<ir::Value> apply(ast::Expression ast_expression) {
        PRAJNA_ASSERT(this);
        return (*this)(ast_expression);
    }

    std::shared_ptr<ir::Value> operator()(boost::blank) {
        PRAJNA_UNREACHABLE;
        return nullptr;
    }

    std::shared_ptr<ir::Value> operator()(ast::CharLiteral ast_char_literal) {
        return ir_utility->create<ir::ConstantChar>(ast_char_literal.value);
    }

    /// @warning 这里后期返回封装好的string类值, 现在先放回一个 char vector
    std::shared_ptr<ir::Value> operator()(ast::StringLiteral ast_string_literal) {
        // 最后需要补零, 以兼容C的字符串
        auto char_string_size = ast_string_literal.value.size() + 1;
        auto ir_char_string_type = ir::ArrayType::create(ir::CharType::create(), char_string_size);
        std::vector<std::shared_ptr<ir::Constant>> ir_inits(ir_char_string_type->size);
        std::transform(RANGE(ast_string_literal.value), ir_inits.begin(),
                       [&](char value) -> std::shared_ptr<ir::Constant> {
                           return ir_utility->create<ir::ConstantChar>(value);
                       });
        // 末尾补零
        ir_inits.back() = ir_utility->create<ir::ConstantChar>('\0');
        auto ir_c_string_constant =
            ir_utility->create<ir::ConstantArray>(ir_char_string_type, ir_inits);
        auto ir_c_string_variable = ir_utility->variableLikedNormalize(ir_c_string_constant);
        auto ir_constant0 = ir_utility->create<ir::ConstantInt>(ir::IntType::create(64, true), 0);

        auto ir_c_string_index0 =
            ir_utility->create<ir::IndexArray>(ir_c_string_variable, ir_constant0);
        auto ir_c_string_address =
            ir_utility->create<ir::GetAddressOfVariableLiked>(ir_c_string_index0);
        ast::IdentifiersResolution ast_identifiers_resolution;
        ast_identifiers_resolution.is_root = ast::Operator("::");
        ast_identifiers_resolution.identifiers = {{std::string("str"), {}}};
        auto string_type = cast<ir::StructType>(
            symbolGet<ir::Type>(this->applyIdentifiersResolution(ast_identifiers_resolution)));
        auto ir_string_from_char_pat = string_type->static_functions["from_char_ptr"];

        return ir_utility->create<ir::Call>(
            ir_string_from_char_pat, std::vector<std::shared_ptr<ir::Value>>{ir_c_string_address});
    }

    std::shared_ptr<ir::Value> operator()(ast::IntLiteral ast_int_literal) {
        auto ir_int_type = ir::IntType::create(64, true);
        return ir_utility->create<ir::ConstantInt>(ir_int_type, ast_int_literal.value);
    };

    std::shared_ptr<ir::Value> operator()(ast::IntLiteralPostfix ast_int_literal_postfix) {
        // -1表示位数和是否有符号尚未确定
        // TODO需要进一步处理
        std::string postfix = ast_int_literal_postfix.postfix;
        auto value = ast_int_literal_postfix.int_literal.value;
        if (postfix == "i8") {
            auto ir_type = ir::IntType::create(8, true);
            return ir_utility->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "i16") {
            auto ir_type = ir::IntType::create(16, true);
            return ir_utility->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "i32") {
            auto ir_type = ir::IntType::create(32, true);
            return ir_utility->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "i64") {
            auto ir_type = ir::IntType::create(64, true);
            return ir_utility->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "u8") {
            auto ir_type = ir::IntType::create(8, false);
            return ir_utility->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "u16") {
            auto ir_type = ir::IntType::create(16, false);
            return ir_utility->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "u32") {
            auto ir_type = ir::IntType::create(32, false);
            return ir_utility->create<ir::ConstantInt>(ir_type, value);
        }
        if (postfix == "u64") {
            auto ir_type = ir::IntType::create(64, false);
            return ir_utility->create<ir::ConstantInt>(ir_type, value);
        }

        auto ir_float = createFloatLiteralPostfix(postfix, static_cast<double>(value));
        if (ir_float) {
            return ir_float;
        }

        PRAJNA_TODO;
        return nullptr;
    };

    std::shared_ptr<ir::Value> createFloatLiteralPostfix(std::string postfix, double value) {
        if (postfix == "f16") {
            auto ir_type = ir::FloatType::create(16);
            return ir_utility->create<ir::ConstantFloat>(ir_type, value);
        }
        if (postfix == "f32") {
            auto ir_type = ir::FloatType::create(32);
            return ir_utility->create<ir::ConstantFloat>(ir_type, value);
        }
        if (postfix == "f64") {
            auto ir_type = ir::FloatType::create(64);
            return ir_utility->create<ir::ConstantFloat>(ir_type, value);
        }

        return nullptr;
    }

    std::shared_ptr<ir::Value> operator()(ast::FloatLiteralPostfix ast_float_literal_postfix) {
        // -1表示位数和是否有符号尚未确定
        // TODO需要进一步处理
        std::string postfix = ast_float_literal_postfix.postfix;
        auto value = ast_float_literal_postfix.float_literal.value;

        auto ir_float = createFloatLiteralPostfix(postfix, value);
        if (ir_float) {
            return ir_float;
        } else {
            PRAJNA_TODO;
            return nullptr;
        }
    };

    std::shared_ptr<ir::Value> operator()(ast::FloatLiteral ast_float_literal) {
        // -1表示位数尚未确定
        // TODO 需要进一步处理
        auto ir_float_type = ir::FloatType::create(32);
        return ir_utility->create<ir::ConstantFloat>(ir_float_type, ast_float_literal.value);
    }

    std::shared_ptr<ir::Value> operator()(ast::BoolLiteral ast_bool_literal) {
        return ir_utility->create<ir::ConstantBool>(ast_bool_literal.value);
    }

    std::shared_ptr<ir::Value> operator()(ast::Null /*ast_null*/) {
        return ir_utility->create<ir::ConstantNull>();
    }

    std::shared_ptr<ir::Value> operator()(ast::Identifier ast_identifier) {
        if (!ir_utility->symbol_table->has(ast_identifier)) {
            PRAJNA_TODO;
        }

        auto symbol = ir_utility->symbol_table->get(ast_identifier);
        if (!symbolIs<ir::Value>(symbol)) {
            PRAJNA_TODO;
        }

        return symbolGet<ir::Value>(symbol);
    }

    std::shared_ptr<ir::Value> operator()(ast::Expression ast_expresion) {
        auto ir_lhs = applyOperand(ast_expresion.first);
        for (auto optoken_and_rhs : ast_expresion.rest) {
            ir_lhs = applyBinaryOperation(ir_lhs, optoken_and_rhs);
        }
        return ir_lhs;
    }

    std::shared_ptr<ir::Value> applyBinaryOperationAccessField(
        std::shared_ptr<ir::Value> ir_lhs, ast::BinaryOperation ast_binary_operation) {
        if (ast_binary_operation.operand.type() != typeid(ast::Identifier)) {
            PRAJNA_TODO;  /// 操作数需要时identifier
        }

        auto ir_type = ir_lhs->type;
        auto ir_variable_liked = ir_utility->variableLikedNormalize(ir_lhs);
        std::string member_name = boost::get<ast::Identifier>(ast_binary_operation.operand);
        if (auto member_function = ir_type->member_functions[member_name]) {
            auto ir_this_pointer =
                ir_utility->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
            return ir::MemberFunction::create(ir_this_pointer, member_function);
        }

        auto iter_field = std::find_if(
            RANGE(ir_type->fields),
            [&](std::shared_ptr<ir::Field> ir_field) { return ir_field->name == member_name; });
        if (iter_field != ir_type->fields.end()) {
            auto ir_field_access =
                ir_utility->create<ir::AccessField>(ir_variable_liked, *iter_field);
            return ir_field_access;
        }

        // 索引property
        for (auto [name, ir_function] : ir_type->member_functions) {
            if (ir_function == nullptr) continue;

            if (ir_function->function_type->annotations.count("property")) {
                if (ir_function->function_type->annotations["property"][0] == member_name) {
                    auto ir_this_pointer =
                        ir_utility->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
                    return ir_utility->create<ir::Property>(ir_this_pointer, member_name);
                }
            }
        }

        PRAJNA_ASSERT(false, member_name);
        return nullptr;
    }

    std::shared_ptr<ir::Value> applyBinaryOperationIndexArray(
        std::shared_ptr<ir::Value> ir_object, ast::BinaryOperation ast_binary_operation) {
        auto ir_variable_liked = ir_utility->variableLikedNormalize(ir_object);
        if (is<ir::ArrayType>(ir_object->type)) {
            auto ir_index = this->applyOperand(ast_binary_operation.operand);
            PRAJNA_ASSERT(is<ir::IntType>(ir_index->type));
            return ir_utility->create<ir::IndexArray>(ir_variable_liked, ir_index);
        } else if (is<ir::PointerType>(ir_object->type)) {
            auto ir_index = this->applyOperand(ast_binary_operation.operand);
            PRAJNA_ASSERT(is<ir::IntType>(ir_index->type));
            return ir_utility->create<ir::IndexPointer>(ir_variable_liked, ir_index);
        } else {
            PRAJNA_TODO;
        }

        PRAJNA_UNREACHABLE;
        return nullptr;
    }

    std::shared_ptr<ir::Value> applyBinaryOperationCall(std::shared_ptr<ir::Value> ir_lhs,
                                                        ast::BinaryOperation ast_binary_operaton) {
        auto ir_arguments = *cast<ir::ValueCollection>(applyOperand(ast_binary_operaton.operand));
        if (auto ir_member_function = cast<ir::MemberFunction>(ir_lhs)) {
            auto ir_argument_collection =
                cast<ir::ValueCollection>(applyOperand(ast_binary_operaton.operand));
            ir_arguments.insert(ir_arguments.begin(), ir_member_function->this_pointer);
            return ir_utility->create<ir::Call>(ir_member_function->global_function, ir_arguments);
        }

        if (ir_lhs->isFunction()) {
            return ir_utility->create<ir::Call>(ir_lhs, ir_arguments);
        }

        if (auto ir_property = cast<ir::Property>(ir_lhs)) {
            ir_property->parent_block->values.remove(ir_property);
            ir_utility->ir_current_block->values.push_back(ir_property);
            ir_property->arguments(ir_arguments);
            return ir_property;
        }

        PRAJNA_TODO;
        return nullptr;
    }

    std::shared_ptr<ir::Value> applyBinaryOperation(std::shared_ptr<ir::Value> ir_lhs,
                                                    ast::BinaryOperation ast_binary_operation) {
        if (ast_binary_operation.operator_ == ast::Operator(".")) {
            return this->applyBinaryOperationAccessField(ir_lhs, ast_binary_operation);
        }
        if (ast_binary_operation.operator_ == ast::Operator("(")) {
            return this->applyBinaryOperationCall(ir_lhs, ast_binary_operation);
        }
        if (ast_binary_operation.operator_ == ast::Operator("[")) {
            return this->applyBinaryOperationIndexArray(ir_lhs, ast_binary_operation);
        } else {
            return this->convertBinaryOperationToCallFunction(ir_lhs, ast_binary_operation);
        }

        PRAJNA_TODO;
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
        return ir_utility->create<ir::DeferencePointer>(ir_operand);
    }

    std::shared_ptr<ir::Value> applyUnaryOperatorGetAddressOfVariableLiked(
        std::shared_ptr<ir::Value> ir_operand) {
        auto ir_variable_liked = cast<ir::VariableLiked>(ir_operand);
        PRAJNA_ASSERT(ir_variable_liked);  // 错误信息
        return ir_utility->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        // return ir_utility->create<ir::DeferencePointer>(ir_operand);
    }

    std::shared_ptr<ir::Value> operator()(ast::Unary ast_unary) {
        auto ir_operand = this->applyOperand(ast_unary.operand);
        if (ast_unary.operator_ == ast::Operator("*")) {
            return this->applyUnaryOperatorDereferencePointer(ir_operand);
        }
        if (ast_unary.operator_ == ast::Operator("&")) {
            return this->applyUnaryOperatorGetAddressOfVariableLiked(ir_operand);
        }

        auto ir_variable_liked = ir_utility->variableLikedNormalize(ir_operand);
        auto member_function_name = ast_unary.operator_.string_token;
        // Prajna不支持函数重载, 故一元算子在前面加"/"和二元算子区分
        auto ir_function = ir_variable_liked->type->member_functions["\\" + member_function_name];
        PRAJNA_ASSERT(ir_function);

        auto ir_this_pointer = ir_utility->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        std::vector<std::shared_ptr<ir::Value>> ir_arguemnts(1);
        ir_arguemnts[0] = ir_this_pointer;
        return ir_utility->create<ir::Call>(ir_function, ir_arguemnts);
    }

    std::shared_ptr<ir::Value> operator()(ast::Cast ast_cast) {
        auto ir_type = this->applyType(ast_cast.type);
        PRAJNA_ASSERT(is<ir::PointerType>(ir_type));
        auto ir_value = this->apply(ast_cast.value);
        PRAJNA_ASSERT(is<ir::PointerType>(ir_value->type));
        return ir_utility->create<ir::BitCast>(ir_value, ir_type);
    }

    std::shared_ptr<ir::Type> applyType(ast::Type ast_postfix_type) {
        auto symbol_type = this->applyIdentifiersResolution(ast_postfix_type.base_type);

        if (!symbolIs<ir::Type>(symbol_type)) {
            PRAJNA_TODO;
        }
        auto ir_type = symbolGet<ir::Type>(symbol_type);

        // @note 有数组类型后需要再处理一下
        for (auto postfix_operator : ast_postfix_type.postfix_type_operators) {
            boost::apply_visitor(
                overloaded{[&](ast::Operator ast_star) {
                               PRAJNA_ASSERT(ast_star == ast::Operator("*"));
                               ir_type = ir::PointerType::create(ir_type);
                           },
                           [&](ast::IntLiteral ast_int_literal) {
                               ir_type = ir::ArrayType::create(ir_type, ast_int_literal.value);
                           },
                           [&](ast::Identifier ast_identifier) {
                               auto symbol_const_int =
                                   ir_utility->symbol_table->get(ast_identifier);
                               auto ir_constant_int = symbolGet<ir::ConstantInt>(symbol_const_int);
                               PRAJNA_ASSERT(ir_constant_int);
                               ir_type = ir::ArrayType::create(ir_type, ir_constant_int->value);
                           }},
                postfix_operator);
        }

        return ir_type;
    }  // namespace prajna::lowering

    Symbol applyIdentifiersResolution(ast::IdentifiersResolution ast_identifiers_resolution) {
        Symbol symbol;
        if (ast_identifiers_resolution.is_root) {
            symbol = ir_utility->symbol_table->rootSymbolTable();
        } else {
            symbol = ir_utility->symbol_table;
        }

        PRAJNA_ASSERT(symbol.which() != 0);

        for (auto iter_ast_identifier = ast_identifiers_resolution.identifiers.begin();
             iter_ast_identifier != ast_identifiers_resolution.identifiers.end();
             ++iter_ast_identifier) {
            symbol = boost::apply_visitor(
                overloaded{
                    [&](auto x) -> Symbol {
                        PRAJNA_UNREACHABLE;
                        return nullptr;
                    },
                    [&](std::shared_ptr<ir::Value> ir_value) -> Symbol {
                        PRAJNA_TODO;
                        return nullptr;
                    },
                    [&](std::shared_ptr<ir::Type> ir_type) -> Symbol {
                        PRAJNA_ASSERT(!iter_ast_identifier->template_arguments, "ERROR");
                        auto ir_static_fun =
                            ir_type->static_functions[iter_ast_identifier->identifier];
                        PRAJNA_ASSERT(ir_static_fun, iter_ast_identifier->identifier);
                        return ir_static_fun;
                    },
                    [&](std::shared_ptr<TemplateStruct> template_strcut) -> Symbol {
                        PRAJNA_UNREACHABLE;
                        return nullptr;
                    },
                    [&](std::shared_ptr<SymbolTable> symbol_table) -> Symbol {
                        auto symbol = symbol_table->get(iter_ast_identifier->identifier);

                        if (!iter_ast_identifier->template_arguments) {
                            return symbol;
                        } else {
                            auto template_struct = symbolGet<TemplateStruct>(symbol);
                            PRAJNA_ASSERT(template_struct);
                            std::list<Symbol> symbol_template_arguments;
                            PRAJNA_ASSERT(iter_ast_identifier->template_arguments);
                            for (auto ast_template_argument :
                                 *iter_ast_identifier->template_arguments) {
                                auto symbol_template_argument = boost::apply_visitor(
                                    overloaded{[&](boost::blank) -> Symbol {
                                                   PRAJNA_UNREACHABLE;
                                                   return nullptr;
                                               },
                                               [&](ast::Type ast_type) -> Symbol {
                                                   if (ast_type.postfix_type_operators.size() ==
                                                       0) {
                                                       return this->applyIdentifiersResolution(
                                                           ast_type.base_type);
                                                   } else {
                                                       return this->applyType(ast_type);
                                                   }
                                               },
                                               [&](ast::IntLiteral ast_int_literal) -> Symbol {
                                                   return ir::ConstantInt::create(
                                                       ir::IntType::create(64, true),
                                                       ast_int_literal.value);
                                               }},
                                    ast_template_argument);
                                symbol_template_arguments.push_back(symbol_template_argument);
                            }
                            auto ir_template_type = template_struct->getStructInstance(
                                symbol_template_arguments, ir_utility->module);
                            PRAJNA_ASSERT(ir_template_type);
                            return ir_template_type;
                        }
                    }},
                symbol);

            if (symbol.which() == 0) {
                auto identifier = iter_ast_identifier->identifier;
                this->logger->log(LogLevel::error, {{CH, fmt::format("{} not found", identifier)}},
                                  identifier.position);
                PRAJNA_TODO;
            }
        }

        return symbol;
    }

    std::shared_ptr<ir::Value> operator()(ast::IdentifiersResolution ast_identifiers_resolution) {
        auto symbol = this->applyIdentifiersResolution(ast_identifiers_resolution);
        PRAJNA_ASSERT(symbol.which() != 0);
        return boost::apply_visitor(
            overloaded{[](std::shared_ptr<ir::Value> ir_value) -> std::shared_ptr<ir::Value> {
                           return ir_value;
                       },
                       [](std::shared_ptr<ir::Type> ir_type) -> std::shared_ptr<ir::Value> {
                           PRAJNA_ASSERT(ir_type->constructor);
                           return ir_type->constructor;
                       },
                       [](std::shared_ptr<ir::ConstantInt> ir_constant_int)
                           -> std::shared_ptr<ir::Value> { return ir_constant_int; },
                       [](auto x) {
                           PRAJNA_TODO;
                           return nullptr;
                       }},
            symbol);
    }

    std::shared_ptr<ir::Value> operator()(ast::Array ast_array) {
        PRAJNA_ASSERT(ast_array.values.size() > 0);
        auto ir_first_value = this->applyOperand(ast_array.values.front());
        auto ir_value_type = ir_first_value->type;
        auto ir_array = this->ir_utility->create<ir::LocalVariable>(
            ir::ArrayType::create(ir_value_type, ast_array.values.size()));
        for (size_t i = 0; i < ast_array.values.size(); ++i) {
            auto ir_idx =
                this->ir_utility->create<ir::ConstantInt>(ir::IntType::create(64, true), i);
            auto ir_index_array = this->ir_utility->create<ir::IndexArray>(ir_array, ir_idx);
            auto ir_value = this->applyOperand(ast_array.values[i]);
            this->ir_utility->create<ir::WriteVariableLiked>(ir_value, ir_index_array);
        }

        return ir_array;
    }

    std::shared_ptr<ir::Value> operator()(ast::KernelFunctionCall ast_kernel_function_call) {
        auto ir_kernel_function = (*this)(ast_kernel_function_call.kernel_function);
        if (ast_kernel_function_call.operation) {
            if (auto ir_function_type = ir_kernel_function->getFunctionType()) {
                auto ir_grid_dim = (*this)(ast_kernel_function_call.operation->grid_dim);
                auto ir_block_dim = (*this)(ast_kernel_function_call.operation->block_dim);
                auto ir_arguments = cast<ir::ValueCollection>(
                    (*this)(ast_kernel_function_call.operation->arguments));
                return this->ir_utility->create<ir::KernelFunctionCall>(
                    ir_kernel_function, ir_grid_dim, ir_block_dim, *ir_arguments);
            } else {
                PRAJNA_TODO;
            }
        } else {
            return ir_kernel_function;
        }

        PRAJNA_UNREACHABLE;
        return nullptr;
    }

    template <typename _Ast>
    std::shared_ptr<ir::Value> operator()(_Ast ast_operand) {
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
        auto ir_variable_liked = ir_utility->variableLikedNormalize(ir_lhs);
        auto member_function_name = ast_binary_operation.operator_.string_token;
        auto ir_function = ir_variable_liked->type->member_functions[member_function_name];
        if (!ir_function) {
            logger->log(LogLevel::error, {{CH, fmt::format("{}函数不存在", member_function_name)}},
                        ast_binary_operation.operator_.position);
        }
        auto ir_this_pointer = ir_utility->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        std::vector<std::shared_ptr<ir::Value>> ir_arguemnts(2);
        ir_arguemnts[0] = ir_this_pointer;
        ir_arguemnts[1] = this->applyOperand(ast_binary_operation.operand);
        return ir_utility->create<ir::Call>(ir_function, ir_arguemnts);
    }

    std::shared_ptr<ir::Value> operator()(ast::SizeOf ast_sizeof) {
        auto ir_type = this->applyType(ast_sizeof.type);
        PRAJNA_ASSERT(ir_type);
        return ir_utility->create<ir::ConstantInt>(ir::IntType::create(64, true), ir_type->bytes);
    }

    bool isArgumentMatched(std::shared_ptr<ir::Value> argument0,
                           std::shared_ptr<ir::Value> argument1) {
        if (argument0->type == argument1->type) {
            return true;
        }

        // TODO 需要兼容位数不定的浮点数 整数等

        return false;
    }

   private:
    std::shared_ptr<IrUtility> ir_utility;
    std::shared_ptr<Logger> logger;
};

}  // namespace prajna::lowering
