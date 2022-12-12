#pragma once

#include <algorithm>
#include <boost/range/combine.hpp>

#include "boost/variant.hpp"
#include "prajna/assert.hpp"
#include "prajna/ast/ast.hpp"
#include "prajna/helper.hpp"
#include "prajna/ir/ir.hpp"
#include "prajna/logger.hpp"
#include "prajna/lowering/builtin.hpp"
#include "prajna/lowering/expression_lowering_visitor.hpp"
#include "prajna/lowering/ir_utility.hpp"
#include "prajna/lowering/symbol_table.hpp"
#include "prajna/lowering/template.hpp"

namespace prajna::lowering {

class StatementLoweringVisitor {
    StatementLoweringVisitor() = default;

    using result_type = Symbol;

   public:
    static std::shared_ptr<StatementLoweringVisitor> create(
        std::shared_ptr<lowering::SymbolTable> symbol_table, std::shared_ptr<Logger> logger,
        std::shared_ptr<ir::Module> ir_module = nullptr) {
        std::shared_ptr<StatementLoweringVisitor> self(new StatementLoweringVisitor);
        if (!ir_module) ir_module = ir::Module::create();
        self->ir_utility = std::make_shared<IrUtility>(symbol_table, ir_module);
        self->logger = logger;
        self->expression_lowering_visitor =
            ExpressionLoweringVisitor::create(self->ir_utility, logger);
        return self;
    }

    std::shared_ptr<ir::Module> apply(std::shared_ptr<ast::Statements> sp_ast_statements) {
        (*this)(*sp_ast_statements);
        ir_utility->module->symbol_table = ir_utility->symbol_table;
        return ir_utility->module;
    }

    Symbol operator()(ast::Statements ast_statements) {
        for (auto ast_statement : ast_statements) {
            boost::apply_visitor(*this, ast_statement);
        }

        return nullptr;
    }

    Symbol operator()(ast::Block block) {
        ir_utility->pushSymbolTableAndBlock();

        (*this)(block.statements);
        auto ir_block = ir_utility->ir_current_block;
        ir_utility->popSymbolTableAndBlock();

        return ir_block;
    }

    Symbol operator()(ast::VariableDeclaration ast_variable_declaration,
                      bool use_global_variable = false) {
        std::shared_ptr<ir::Type> ir_type = nullptr;
        std::shared_ptr<ir::Value> ir_initial_value = nullptr;
        if (ast_variable_declaration.type) {
            ir_type = this->applyType(*ast_variable_declaration.type);
        }
        //确定变量的type
        if (ast_variable_declaration.initialize) {
            ir_initial_value =
                expression_lowering_visitor->apply((*ast_variable_declaration.initialize).value);

            if (!ir_type) {
                ir_type = ir_initial_value->type;
            } else {
                if (ir_type != ir_initial_value->type) {
                    logger->log(LogLevel::error, {{CH, std::string("类型不匹配")}},
                                (*ast_variable_declaration.initialize).assign_operator.position);
                    throw CompileError::create();
                }
            }
        }

        // 类型不确定
        if (!ir_type) {
            PRAJNA_TODO;
        }

        std::shared_ptr<ir::VariableLiked> ir_variable_liked;
        if (use_global_variable) {
            auto ir_global_variable = ir::GlobalVariable::create(ir_type);
            ir_global_variable->parent_module = ir_utility->module;
            ir_utility->module->global_variables.push_back(ir_global_variable);
            ir_variable_liked = ir_global_variable;
        } else {
            ir_variable_liked = ir_utility->create<ir::LocalVariable>(ir_type);
        }
        ir_utility->symbol_table->setWithName(ir_variable_liked, ast_variable_declaration.name);

        if (ir_initial_value) {
            auto write_variable =
                ir_utility->create<ir::WriteVariableLiked>(ir_initial_value, ir_variable_liked);
        }

        return ir_variable_liked;
    }

    Symbol operator()(ast::Assignment ast_assignment) {
        // 右值应该先解析
        auto ir_rhs = expression_lowering_visitor->applyOperand(ast_assignment.right);
        auto ir_lhs = expression_lowering_visitor->applyOperand(ast_assignment.left);

        if (auto ir_variable_liked = cast<ir::VariableLiked>(ir_lhs)) {
            ir_utility->create<ir::WriteVariableLiked>(ir_rhs, ir_variable_liked);
            return ir_lhs;
        }

        if (auto ir_property = cast<ir::Property>(ir_lhs)) {
            ir_utility->create<ir::WriteProperty>(ir_rhs, ir_property);
            return ir_lhs;
        }

        PRAJNA_TODO;
        return nullptr;
    }

    std::vector<std::shared_ptr<ir::Type>> applyParameters(
        std::vector<ast::Parameter> ast_parameters) {
        std::vector<std::shared_ptr<ir::Type>> types(ast_parameters.size());
        std::transform(RANGE(ast_parameters), types.begin(), [&](ast::Parameter ast_parameter) {
            return this->applyType(ast_parameter.type);
        });

        return types;
    }

    auto applyAnnotations(ast::Annotations ast_annotations) {
        std::map<std::string, std::vector<std::string>> annotations;
        for (auto ast_annotation : ast_annotations) {
            annotations.insert({ast_annotation.property, ast_annotation.values});
        }
        return annotations;
    }

    bool verifyReturnType(std::shared_ptr<ir::Block> ir_block, std::shared_ptr<ir::Type> ir_type) {
        PRAJNA_ASSERT(!is<ir::VoidType>(ir_type));
        auto ir_last_value = ir_block->values.back();
        if (auto ir_return = cast<ir::Return>(ir_last_value)) {
            return ir_return->value() && ir_return->value()->type == ir_type;
        }
        if (auto ir_if = cast<ir::If>(ir_last_value)) {
            auto re = this->verifyReturnType(ir_if->trueBlock(), ir_type) &&
                      this->verifyReturnType(ir_if->falseBlock(), ir_type);
            if (re) {
                // //插入一个Return避免错误
                // auto ir
                auto ir_variable = this->ir_utility->create<ir::LocalVariable>(ir_type);
                auto ir_return = this->ir_utility->create<ir::Return>(ir_variable);
                ir_if->parent_block->pushBack(ir_return);
                return true;
            } else {
                return false;
            }
        }
        if (auto ir_block_ = cast<ir::Block>(ir_last_value)) {
            return this->verifyReturnType(ir_block_, ir_type);
        }

        return false;
    }

    std::shared_ptr<ir::Function> applyFunctionHeader(
        ast::FunctionHeader ast_function_header, std::shared_ptr<ir::Type> ir_this_pointer_type) {
        std::shared_ptr<ir::Type> return_type;
        if (ast_function_header.return_type) {
            return_type = applyType(*ast_function_header.return_type);
        } else {
            return_type = ir::VoidType::create();
        }
        auto ir_argument_types = applyParameters(ast_function_header.parameters);
        // 插入this-pointer type
        if (ir_this_pointer_type) {
            ir_argument_types.insert(ir_argument_types.begin(), ir_this_pointer_type);
        }

        auto ir_function_type = ir::FunctionType::create(return_type, ir_argument_types);
        auto ir_function = ir::Function::create(ir_function_type);
        ir_function_type->function = ir_function;
        ir_function->parent_module = ir_utility->module;
        ir_utility->module->functions.push_back(ir_function);

        // TODO interface里需要处理一下
        ir_utility->symbol_table->setWithName(ir_function, ast_function_header.name);
        ir_function_type->name = ir_function->name;
        ir_function_type->fullname = ir_function->fullname;
        ir_function_type->annotations = this->applyAnnotations(ast_function_header.annotations);

        return ir_function;
    }

    Symbol operator()(ast::Function ast_function,
                      std::shared_ptr<ir::Type> ir_this_poiner_type = nullptr) {
        auto ir_function = applyFunctionHeader(ast_function.declaration, ir_this_poiner_type);

        ir_utility->ir_current_function = ir_function;
        // 进入参数域,
        ir_utility->pushSymbolTable();

        // @note 将function的第一个block作为最上层的block
        auto ir_block = ir::Block::create();
        ir_block->parent_function = ir_function;
        ir_utility->ir_current_block = ir_block;
        ir_function->blocks.push_back(ir_block);

        size_t j = 0;
        if (ir_this_poiner_type) {
            // Argument也不会插入的block里
            auto ir_this_pointer = ir_utility->create<ir::Argument>(ir_this_poiner_type);
            ir_function->arguments.push_back(ir_this_pointer);
            ir_utility->symbol_table->setWithName(ir_this_pointer, "this-pointer");
            ++j;
        }
        for (size_t i = 0; i < ast_function.declaration.parameters.size(); ++i, ++j) {
            auto ir_argument_type = ir_function->function_type->argument_types[j];
            auto ir_argument = ir_utility->create<ir::Argument>(ir_argument_type);
            ir_function->arguments.push_back(ir_argument);
            ir_utility->symbol_table->setWithName(ir_argument,
                                                  ast_function.declaration.parameters[i].name);
        }

        if (ast_function.body) {
            if (ir_this_poiner_type) {
                auto ir_this_pointer =
                    symbolGet<ir::Value>(ir_utility->symbol_table->get("this-pointer"));
                auto ir_this = ir_utility->create<ir::ThisWrapper>(ir_this_pointer);
                ir_utility->symbol_table->setWithName(ir_this, "this");
            }

            (*this)(*ast_function.body);

            // void返回类型直接补一个Return即可, 让后端去优化冗余的指令
            if (is<ir::VoidType>(ir_function->function_type->return_type)) {
                ir_utility->create<ir::Return>(ir::VoidValue::create());
            } else {
                PRAJNA_ASSERT(this->verifyReturnType(ir_function->blocks.back(),
                                                     ir_function->function_type->return_type),
                              "Error");
            }
        } else {
            ir_function->function_type->annotations.insert({"declare", {}});
        }

        ir_utility->ir_current_block = nullptr;
        ir_utility->popSymbolTable();

        return ir_function;
    }

    Symbol operator()(ast::Return ast_return) {
        std::shared_ptr<ir::Return> ir_return;
        if (ast_return.expr) {
            auto ir_return_value = expression_lowering_visitor->apply(*ast_return.expr);
            ir_return = ir_utility->create<ir::Return>(ir_return_value);
        } else {
            auto ir_void_value = ir_utility->create<ir::VoidValue>();
            ir_return = ir_utility->create<ir::Return>(ir_void_value);
        }

        return ir_return;
    }

    Symbol operator()(ast::If ast_if) {
        auto ir_condition = applyExpression(ast_if.condition);
        auto ir_if =
            ir_utility->create<ir::If>(ir_condition, ir::Block::create(), ir::Block::create());

        ir_utility->pushBlock(ir_if->trueBlock());
        (*this)(ast_if.then);
        ir_utility->popBlock(ir_if->trueBlock());

        ir_utility->pushBlock(ir_if->falseBlock());
        if (ast_if.else_) {
            (*this)(*ast_if.else_);
        }
        ir_utility->popBlock(ir_if->falseBlock());

        return ir_if;
    }

    Symbol operator()(ast::While ast_while) {
        auto ir_condition_block = ir::Block::create();
        ir_utility->pushBlock(ir_condition_block);
        auto ir_condition = applyExpression(ast_while.condition);
        ir_utility->popBlock(ir_condition_block);

        auto ir_while =
            ir_utility->create<ir::While>(ir_condition, ir_condition_block, ir::Block::create());

        ir_utility->pushBlock(ir_while->loopBlock());
        (*this)(ast_while.body);
        ir_utility->popBlock(ir_while->loopBlock());

        return ir_while;
    }

    Symbol operator()(ast::For ast_grid) {
      // TODO 后面看下如何处理类型的问题
      auto ir_first = expression_lowering_visitor->apply(ast_grid.first);
      auto ir_last = expression_lowering_visitor->apply(ast_grid.last);
      auto ir_index =
          ir_utility->create<ir::LocalVariable>(ir::IntType::create(64, true));
      ir_utility->symbol_table->setWithName(ir_index, ast_grid.index);
      PRAJNA_ASSERT(ir_first->type == ir_last->type &&
                    ir_first->type == ir_index->type);

      auto ir_loop_block = ir::Block::create();
      auto ir_for = ir_utility->create<ir::For>(ir_index, ir_first, ir_last,
                                                ir_loop_block);

      ir_utility->pushBlock(ir_for->loopBlock());
      (*this)(ast_grid.body);
      ir_utility->popBlock(ir_for->loopBlock());

      for (auto ast_annotation : ast_grid.annotations) {
        ir_for->annotations.insert(
            {ast_annotation.property, ast_annotation.values});
      }
      return ir_for;
    }

    void createStructConstructor(std::shared_ptr<ir::StructType> ir_struct_type) {
        auto ir_fields = ir_struct_type->fields;
        std::vector<std::shared_ptr<ir::Type>> ir_constructor_arg_types(ir_fields.size());
        std::transform(RANGE(ir_fields), ir_constructor_arg_types.begin(),
                       [](auto ir_field) { return ir_field->type; });
        auto ir_constructor_type =
            ir::FunctionType::create(ir_struct_type, ir_constructor_arg_types);
        auto ir_constructor = ir::Function::create(ir_constructor_type);
        ir_constructor_type->function = ir_constructor;
        ir_struct_type->constructor = ir_constructor;
        ir_constructor->name = concatFullname(ir_struct_type->name, "constructor");
        ir_constructor->fullname = ir_constructor->name;
        ir_constructor->parent_module = ir_utility->module;
        ir_constructor->arguments.resize(ir_constructor_type->argument_types.size());
        for (size_t i = 0; i < ir_constructor->arguments.size(); ++i) {
            ir_constructor->arguments[i] =
                ir::Argument::create(ir_constructor_type->argument_types[i]);
        }
        ir_utility->module->functions.push_back(ir_constructor);
        auto ir_block = ir::Block::create();
        ir_block->parent_function = ir_constructor;
        ir_constructor->blocks.push_back(ir_block);
        ir_utility->ir_current_block = ir_block;
        auto ir_variable = ir_utility->create<ir::LocalVariable>(ir_struct_type);
        for (size_t i = 0; i < ir_fields.size(); ++i) {
            auto ir_field = ir_fields[i];
            auto ir_access_field = ir_utility->create<ir::AccessField>(ir_variable, ir_field);
            ir_utility->create<ir::WriteVariableLiked>(ir_constructor->arguments[i],
                                                       ir_access_field);
        }
        ir_utility->create<ir::Return>(ir_variable);
        ir_utility->ir_current_block = nullptr;
    }

    std::shared_ptr<ir::Type> applyType(ast::Type ast_postfix_type) {
        return expression_lowering_visitor->applyType(ast_postfix_type);
    }

    std::shared_ptr<ir::StructType> applyStructWithOutTemplates(
        std::shared_ptr<ir::StructType> ir_struct_type, ast::Struct ast_struct) {
        std::vector<std::shared_ptr<ir::Field>> ir_fields(ast_struct.fields.size());
        for (size_t i = 0; i < ast_struct.fields.size(); ++i) {
            ir_fields[i] = ir::Field::create(ast_struct.fields[i].name,
                                             this->applyType(ast_struct.fields[i].type), i);
        }

        ir_struct_type->fields = ir_fields;
        ir_struct_type->update();
        this->createStructConstructor(ir_struct_type);

        return ir_struct_type;
    }

    std::list<ast::Identifier> getTemplateParametersIdentifiers(
        std::list<ast::TemplateParameter> template_parameters) {
        std::set<ast::Identifier> template_parameter_identifier_set;
        std::list<ast::Identifier> template_parameter_identifier_list;
        for (auto template_parameter : template_parameters) {
            template_parameter_identifier_list.push_back(template_parameter);
            template_parameter_identifier_set.insert(template_parameter);
        }
        PRAJNA_ASSERT(template_parameter_identifier_list.size() ==
                      template_parameter_identifier_set.size());

        return template_parameter_identifier_list;
    }

    Symbol operator()(ast::Struct ast_struct) {
        if (ast_struct.template_parameters.empty()) {
            auto ir_struct_type = ir::StructType::create({});
            ir_utility->symbol_table->setWithName(ir_struct_type, ast_struct.name);
            return this->applyStructWithOutTemplates(ir_struct_type, ast_struct);
        } else {
            auto template_parameter_identifier_list =
                getTemplateParametersIdentifiers(ast_struct.template_parameters);

            auto symbol_table = ir_utility->symbol_table;
            auto logger = this->logger;

            auto template_struct = std::make_shared<TemplateStruct>();

            // 外部使用算子, 必须值捕获
            auto template_struct_generator =
                [symbol_table, logger, ast_struct, template_parameter_identifier_list,
                 template_struct](
                    std::list<Symbol> symbol_list,
                    std::shared_ptr<ir::Module> ir_module) -> std::shared_ptr<ir::StructType> {
                // 包裹一层名字空间, 避免被污染
                auto templates_symbol_table = SymbolTable::create(symbol_table);
                PRAJNA_ASSERT(template_parameter_identifier_list.size() == symbol_list.size());

                std::string symbol_list_fullname = "<";
                for (auto [identifier, symbol] :
                     boost::combine(template_parameter_identifier_list, symbol_list)) {
                    templates_symbol_table->set(symbol, identifier);
                    symbol_list_fullname.append(symbolGetFullname(symbol));
                    symbol_list_fullname.push_back(',');
                }
                if (symbol_list_fullname.back() == ',') symbol_list_fullname.pop_back();
                symbol_list_fullname.push_back('>');

                auto ir_struct_type = ir::StructType::create({});
                ir_struct_type->name = ast_struct.name + symbol_list_fullname;
                ir_struct_type->fullname =
                    concatFullname(symbol_table->fullname(), ir_struct_type->name);
                // @需要提前把ir_struct_type设置, 这样才能嵌套
                template_struct->struct_type_instance_dict[symbol_list] = ir_struct_type;

                auto statement_lowering_visitor =
                    StatementLoweringVisitor::create(templates_symbol_table, logger, ir_module);
                return statement_lowering_visitor->applyStructWithOutTemplates(ir_struct_type,
                                                                               ast_struct);
            };

            template_struct->template_struct_impl =
                Template<ir::StructType>::create(template_struct_generator);
            // auto template_struct = TemplateStruct::create(template_struct_generator);

            ir_utility->symbol_table->setWithName(template_struct, ast_struct.name);

            return template_struct;
        }
    }

    void applyImplementStructWithOutTemplates(std::shared_ptr<ir::Type> ir_type,
                                              ast::ImplementStruct ast_implement_struct) {
        ir_utility->pushSymbolTable();
        for (auto ast_function : ast_implement_struct.functions) {
            std::shared_ptr<ir::Function> ir_function = nullptr;
            if (std::none_of(RANGE(ast_function.declaration.annotations),
                             [](auto x) { return x.property == "static"; })) {
                auto ir_this_pointer_type = ir::PointerType::create(ir_type);
                auto symbol_function = (*this)(ast_function, ir_this_pointer_type);
                ir_function = cast<ir::Function>(symbolGet<ir::Value>(symbol_function));
                ir_function->function_type->annotations.insert({"member", {}});
                // TODO需要修复
                // PRAJNA_ASSERT(ir_type->member_functions.count(ir_function->name) == 0);
                ir_type->member_functions[ir_function->name] = ir_function;
                ir_function->fullname = concatFullname(ir_type->fullname, ir_function->name);
            } else {
                if (ast_function.declaration.name == "initialize") {
                    int a = 1 + 3;
                }
                ir_utility->pushSymbolTable();
                ir_utility->symbol_table->name =
                    ir_type->name;  // 插入类的名字, 以便函数名字是正确的
                auto symbol_function = (*this)(ast_function);
                ir_function = cast<ir::Function>(symbolGet<ir::Value>(symbol_function));
                ir_utility->popSymbolTable();
                ir_type->static_functions[ir_function->name] = ir_function;
            }

            std::set<std::string> copy_destroy_callback_names = {"initialize", "copy", "destroy"};
            if (copy_destroy_callback_names.count(ir_function->name)) {
                PRAJNA_ASSERT(ir_function->function_type->annotations.count("static") == 0);
                PRAJNA_ASSERT(is<ir::VoidType>(ir_function->function_type->return_type));
                // this pointer argument
                PRAJNA_ASSERT(ir_function->function_type->argument_types.size() == 1);
                if (ir_function->name == "initialize") {
                    ir_type->initialize_function = ir_function;
                }
                if (ir_function->name == "copy") {
                    ir_type->copy_function = ir_function;
                }
                if (ir_function->name == "destroy") {
                    ir_type->destroy_function = ir_function;
                }
            }
        }

        ir_utility->popSymbolTable();
    }

    Symbol operator()(ast::ImplementStruct ast_implement_struct) {
        if (ast_implement_struct.template_paramters.empty()) {
            auto ir_symbol = expression_lowering_visitor->applyIdentifiersResolution(
                ast_implement_struct.struct_);
            auto ir_type = symbolGet<ir::Type>(ir_symbol);
            PRAJNA_ASSERT(ir_type);
            this->applyImplementStructWithOutTemplates(ir_type, ast_implement_struct);

        } else {
            auto template_parameter_identifier_list =
                getTemplateParametersIdentifiers(ast_implement_struct.template_paramters);

            auto ir_symbol = expression_lowering_visitor->applyIdentifiersResolution(
                ast_implement_struct.struct_);
            auto template_struct = symbolGet<TemplateStruct>(ir_symbol);
            PRAJNA_ASSERT(template_struct);

            auto symbol_table = ir_utility->symbol_table;
            auto logger = this->logger;
            auto template_implement_struct_generator =
                [symbol_table, logger, ast_implement_struct, template_struct,
                 template_parameter_identifier_list](std::list<Symbol> symbol_list,
                                                     std::shared_ptr<ir::Module> ir_module) {
                    // 包裹一层名字空间, 避免被污染
                    auto templates_symbol_table = SymbolTable::create(symbol_table);
                    PRAJNA_ASSERT(template_parameter_identifier_list.size() == symbol_list.size());
                    for (auto [identifier, symbol] :
                         boost::combine(template_parameter_identifier_list, symbol_list)) {
                        templates_symbol_table->set(symbol, identifier);
                    }
                    auto statement_lowering_visitor =
                        StatementLoweringVisitor::create(templates_symbol_table, logger, ir_module);
                    auto ir_type = template_struct->struct_type_instance_dict[symbol_list];
                    statement_lowering_visitor->applyImplementStructWithOutTemplates(
                        ir_type, ast_implement_struct);
                    // 仅仅用于标记是否实例化, 没有实际用途
                    return std::shared_ptr<ImplementTag>(new ImplementTag);
                };
            auto template_implement_struct =
                Template<ImplementTag>::create(template_implement_struct_generator);
            template_struct->pushBackImplements(template_implement_struct);
        }

        return nullptr;
    }

    Symbol operator()(ast::Expression ast_expression) {
        return this->applyExpression(ast_expression);
    }

    Symbol applyIdentifiersResolution(ast::IdentifiersResolution ast_identifiers_resolution) {
        return expression_lowering_visitor->applyIdentifiersResolution(ast_identifiers_resolution);
    }

    Symbol operator()(ast::Import ast_import) {
        auto symbol = this->applyIdentifiersResolution(ast_import.identifiers_resolution);
        PRAJNA_ASSERT(symbol.which() != 0);  // TODO ERROR

        PRAJNA_ASSERT(!ast_import.identifiers_resolution.identifiers.empty());
        // setWithName 不能改变symbol的名字
        ir_utility->symbol_table->set(
            symbol, ast_import.identifiers_resolution.identifiers.back().identifier);

        if (ast_import.as) {
            ir_utility->symbol_table->set(symbol, *ast_import.as);
        }

        return symbol;
    }

    Symbol operator()(ast::Export ast_export) {
        auto symbol = ir_utility->symbol_table->get(ast_export.identifier);
        PRAJNA_ASSERT(symbol.which() != 0);

        auto parent_symbol_table = ir_utility->symbol_table->parent_symbol_table;
        PRAJNA_ASSERT(parent_symbol_table, "不可能为nullptr, 因为文件名本身也是一层名字空间");
        PRAJNA_ASSERT(!parent_symbol_table->currentTableHas(ast_export.identifier), "error");
        parent_symbol_table->set(symbol, ast_export.identifier);

        return symbol;
    }

template <typename _Statement>
Symbol operator()(_Statement ast_statement) {
    PRAJNA_ASSERT(false, typeid(ast_statement).name());
    return nullptr;
}

std::shared_ptr<ir::Value> applyExpression(ast::Expression ast_expression) {
    return expression_lowering_visitor->apply(ast_expression);
}

public:
std::shared_ptr<ExpressionLoweringVisitor> expression_lowering_visitor = nullptr;
std::shared_ptr<IrUtility> ir_utility = nullptr;
std::shared_ptr<Logger> logger = nullptr;
};

}  // namespace prajna::lowering
