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
#include "prajna/lowering/ir_builder.hpp"
#include "prajna/lowering/symbol_table.hpp"
#include "prajna/lowering/template.hpp"

namespace prajna {
class Compiler;
}  // namespace prajna

namespace prajna::lowering {

class StatementLoweringVisitor {
    StatementLoweringVisitor() = default;

    using result_type = Symbol;

   public:
    static std::shared_ptr<StatementLoweringVisitor> create(
        std::shared_ptr<lowering::SymbolTable> symbol_table, std::shared_ptr<Logger> logger,
        std::shared_ptr<ir::Module> ir_module, std::shared_ptr<Compiler> compiler) {
        std::shared_ptr<StatementLoweringVisitor> self(new StatementLoweringVisitor);
        if (!ir_module) ir_module = ir::Module::create();
        self->ir_builder = IrBuilder::create(symbol_table, ir_module, logger);
        self->logger = logger;
        self->compiler = compiler;
        self->expression_lowering_visitor =
            ExpressionLoweringVisitor::create(self->ir_builder, logger);
        return self;
    }

    std::shared_ptr<ir::Module> apply(std::shared_ptr<ast::Statements> sp_ast_statements) {
        (*this)(*sp_ast_statements);
        ir_builder->module->symbol_table = ir_builder->symbol_table;
        return ir_builder->module;
    }

    Symbol operator()(ast::Module ast_module) {
        auto symbol_table = ir_builder->symbol_table;
        for (auto ast_template_identifier : ast_module.name.identifiers) {
            if (ast_template_identifier.template_arguments) {
                logger->error("the invalid module name",
                              *ast_template_identifier.template_arguments);
            }
            auto ast_module_name = ast_template_identifier.identifier;
            if (!symbol_table->currentTableHas(ast_module_name)) {
                symbol_table->set(SymbolTable::create(symbol_table), ast_module_name);
            }
            symbol_table = symbolGet<SymbolTable>(symbol_table->get(ast_module_name));
            if (!symbol_table) {
                logger->error("is not a module", ast_module_name);
            }
        }

        try {
            auto pre_symbole_table = ir_builder->symbol_table;
            ir_builder->symbol_table = symbol_table;
            for (auto ast_statement : ast_module.statements) {
                (*this)(ast_statement);
            }
            ir_builder->symbol_table = pre_symbole_table;

        } catch (CompileError compile_error) {
            /// TODO 需要进一步处理, Compiler的error计数并未生效;
        }

        return symbol_table;
    }

    Symbol operator()(ast::Statement ast_statement) {
        return boost::apply_visitor(*this, ast_statement);
    }

    Symbol operator()(ast::Statements ast_statements) {
        for (auto ast_statement : ast_statements) {
            boost::apply_visitor(*this, ast_statement);
        }

        return nullptr;
    }

    Symbol operator()(ast::Block block) {
        ir_builder->pushSymbolTable();
        ir_builder->createAndPushBlock();

        (*this)(block.statements);
        auto ir_block = ir_builder->currentBlock();
        ir_builder->popSymbolTableAndBlock();

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
                expression_lowering_visitor->apply(*ast_variable_declaration.initialize);

            if (!ir_type) {
                ir_type = ir_initial_value->type;
            } else {
                if (ir_type != ir_initial_value->type) {
                    logger->error(fmt::format("the declaration type is \"{}\", but the initialize "
                                              "value's type is \"{}\"",
                                              ir_type->name, ir_initial_value->type->name),
                                  *ast_variable_declaration.initialize);
                }
            }
        }

        // 类型不确定
        if (!ir_type) {
            logger->error("the declaration type is unsure", ast_variable_declaration);
        }

        std::shared_ptr<ir::VariableLiked> ir_variable_liked;
        if (use_global_variable) {
            auto ir_global_variable = ir::GlobalVariable::create(ir_type);
            ir_global_variable->parent_module = ir_builder->module;
            ir_builder->module->global_variables.push_back(ir_global_variable);
            ir_variable_liked = ir_global_variable;
        } else {
            ir_variable_liked = ir_builder->create<ir::LocalVariable>(ir_type);
        }
        ir_builder->setSymbolWithAssigningName(ir_variable_liked, ast_variable_declaration.name);

        if (ir_initial_value) {
            auto write_variable =
                ir_builder->create<ir::WriteVariableLiked>(ir_initial_value, ir_variable_liked);
        }

        return ir_variable_liked;
    }

    Symbol operator()(ast::Assignment ast_assignment) {
        // 右值应该先解析
        auto ir_rhs = expression_lowering_visitor->applyOperand(ast_assignment.right);
        auto ir_lhs = expression_lowering_visitor->applyOperand(ast_assignment.left);

        if (auto ir_variable_liked = cast<ir::VariableLiked>(ir_lhs)) {
            if (ir_variable_liked->type != ir_rhs->type) {
                logger->error("the type is not matched", ast_assignment);
            }
            ir_builder->create<ir::WriteVariableLiked>(ir_rhs, ir_variable_liked);
            return ir_lhs;
        }

        if (auto ir_property = cast<ir::AccessProperty>(ir_lhs)) {
            if (not ir_property->property->set_function) {
                logger->error("the property has not a setter function", ast_assignment.left);
            }
            if (ir_property->property->set_function->function_type->parameter_types.back() !=
                ir_rhs->type) {
                logger->error("the type is not matched", ast_assignment);
            }
            ir_builder->create<ir::WriteProperty>(ir_rhs, ir_property);
            return ir_lhs;
        }

        logger->error("the left side is not writeable", ast_assignment.left);

        return nullptr;
    }

    std::list<std::shared_ptr<ir::Type>> applyParameters(std::list<ast::Parameter> ast_parameters) {
        std::list<std::shared_ptr<ir::Type>> types(ast_parameters.size());
        std::transform(RANGE(ast_parameters), types.begin(), [=](ast::Parameter ast_parameter) {
            return this->applyType(ast_parameter.type);
        });

        return types;
    }

    auto applyAnnotations(ast::AnnotationDict ast_annotations) {
        std::unordered_map<std::string, std::list<std::string>> annotation_dict;
        for (auto ast_annotation : ast_annotations) {
            std::list<std::string> values;
            std::transform(RANGE(ast_annotation.values), std::back_inserter(values),
                           [](ast::StringLiteral string_literal) { return string_literal.value; });
            annotation_dict.insert({ast_annotation.name, values});
        }
        return annotation_dict;
    }

    bool allBranchIsTerminated(std::shared_ptr<ir::Block> ir_block,
                               std::shared_ptr<ir::Type> ir_type) {
        PRAJNA_ASSERT(!is<ir::VoidType>(ir_type));
        auto ir_last_value = ir_block->values.back();
        if (auto ir_return = cast<ir::Return>(ir_last_value)) {
            return ir_return->value() && ir_return->value()->type == ir_type;
        }
        if (auto ir_if = cast<ir::If>(ir_last_value)) {
            auto re = this->allBranchIsTerminated(ir_if->trueBlock(), ir_type) &&
                      this->allBranchIsTerminated(ir_if->falseBlock(), ir_type);
            if (re) {
                // //插入一个Return协助判断
                ir_builder->pushBlock(ir_if->parent_block);
                auto ir_variable = ir_builder->create<ir::LocalVariable>(ir_type);
                auto ir_return = ir_builder->create<ir::Return>(ir_variable);
                ir_builder->popBlock();

                return true;
            } else {
                return false;
            }
        }
        if (auto ir_block_ = cast<ir::Block>(ir_last_value)) {
            return this->allBranchIsTerminated(ir_block_, ir_type);
        }

        return false;
    }

    std::shared_ptr<ir::Function> applyFunctionHeader(ast::FunctionHeader ast_function_header) {
        std::shared_ptr<ir::Type> return_type;
        if (ast_function_header.return_type) {
            return_type = applyType(*ast_function_header.return_type);
        } else {
            return_type = ir::VoidType::create();
        }
        auto ir_argument_types = applyParameters(ast_function_header.parameters);
        // 插入this-pointer type
        if (ir_builder->isBuildingMemberfunction()) {
            ir_argument_types.insert(ir_argument_types.begin(), ir_builder->this_pointer_type);
        }

        auto ir_function_type = ir::FunctionType::create(ir_argument_types, return_type);

        // 如果已经声明过该函数
        if (ir_builder->symbol_table->currentTableHas(ast_function_header.name)) {
            auto ir_function_declaration = cast<ir::Function>(
                symbolGet<ir::Value>(ir_builder->symbol_table->get(ast_function_header.name)));
            if (ir_function_declaration->is_declaration) {
                if (ir_function_type != ir_function_declaration->function_type) {
                    logger->error("the declarated function type is not matched",
                                  ast_function_header.name);
                }

                ir_function_declaration->blocks.clear();
                ir_function_declaration->annotation_dict =
                    this->applyAnnotations(ast_function_header.annotation_dict);

                return ir_function_declaration;
            }
        }

        auto ir_function = ir_builder->createFunction(ast_function_header.name, ir_function_type);
        ir_function->annotation_dict = this->applyAnnotations(ast_function_header.annotation_dict);

        return ir_function;
    }

    Symbol operator()(ast::Function ast_function) {
        try {
            ir_builder->is_static_function =
                std::any_of(RANGE(ast_function.declaration.annotation_dict),
                            [](auto x) { return x.name == "static"; });

            auto ir_function = applyFunctionHeader(ast_function.declaration);
            ir_function->source_location = ast_function.declaration.name;
            // 进入参数域,

            // @note 将function的第一个block作为最上层的block
            if (ast_function.body) {
                ir_builder->pushSymbolTable();
                ir_builder->createTopBlockForFunction(ir_function);

                auto iter_argument = ir_function->parameters.begin();
                if (ir_builder->isBuildingMemberfunction()) {
                    // Argument也不会插入的block里
                    ir_builder->symbol_table->setWithAssigningName(ir_function->parameters.front(),
                                                                   "this-pointer");
                    ++iter_argument;
                }

                for (auto iter_parameter = ast_function.declaration.parameters.begin();
                     iter_parameter != ast_function.declaration.parameters.end();
                     ++iter_argument, ++iter_parameter) {
                    ir_builder->setSymbolWithAssigningName(*iter_argument, iter_parameter->name);
                }

                if (ir_builder->isBuildingMemberfunction()) {
                    auto ir_this_pointer =
                        symbolGet<ir::Value>(ir_builder->symbol_table->get("this-pointer"));
                    auto ir_this = ir_builder->create<ir::ThisWrapper>(ir_this_pointer);
                    ir_builder->symbol_table->setWithAssigningName(ir_this, "this");
                }

                (*this)(*ast_function.body);

                // TODO 返回型需要进一步处理
                // void返回类型直接补一个Return即可, 让后端去优化冗余的指令
                if (is<ir::VoidType>(ir_function->function_type->return_type)) {
                    ir_builder->create<ir::Return>(ir_builder->create<ir::VoidValue>());
                } else {
                    if (not this->allBranchIsTerminated(ir_function->blocks.back(),
                                                        ir_function->function_type->return_type)) {
                        logger->error("not all branch have been terminated",
                                      ast_function.declaration);
                    }
                }

                ir_builder->popBlock();
                ir_builder->function_stack.pop();
                ir_builder->popSymbolTable();
                ir_function->is_declaration = false;
            } else {
                ir_function->is_declaration = true;
            }

            ir_builder->is_static_function = false;

            return ir_function;
        } catch (CompileError compile_error) {
            logger->note(ast_function.declaration);
            throw compile_error;
        }
    }

    Symbol operator()(ast::Return ast_return) {
        std::shared_ptr<ir::Return> ir_return;
        if (ast_return.expr) {
            auto ir_return_value = expression_lowering_visitor->apply(*ast_return.expr);
            ir_return = ir_builder->create<ir::Return>(ir_return_value);
        } else {
            auto ir_void_value = ir_builder->create<ir::VoidValue>();
            ir_return = ir_builder->create<ir::Return>(ir_void_value);
        }

        auto ir_return_type = ir_builder->function_stack.top()->function_type->return_type;
        if (ir_return->type != ir_return_type) {
            logger->error(fmt::format("the type is {} , but then function return type is {}",
                                      ir_return->type->fullname, ir_return_type->fullname),
                          ast_return);
        }

        return ir_return;
    }

    Symbol operator()(ast::If ast_if) {
        auto ir_condition = applyExpression(ast_if.condition);
        auto ir_if =
            ir_builder->create<ir::If>(ir_condition, ir::Block::create(), ir::Block::create());
        ir_if->trueBlock()->parent_function = ir_builder->function_stack.top();
        ir_if->falseBlock()->parent_function = ir_builder->function_stack.top();

        ir_builder->pushBlock(ir_if->trueBlock());
        (*this)(ast_if.then);
        ir_builder->popBlock();

        ir_builder->pushBlock(ir_if->falseBlock());
        if (ast_if.else_) {
            (*this)(*ast_if.else_);
        }
        ir_builder->popBlock();

        return ir_if;
    }

    Symbol operator()(ast::While ast_while) {
        auto ir_loop_before = ir::Label::create();
        ir_builder->loop_before_label_stack.push(ir_loop_before);
        auto ir_loop_after = ir::Label::create();
        ir_builder->loop_after_label_stack.push(ir_loop_after);

        auto ir_condition_block = ir::Block::create();
        ir_condition_block->parent_function = ir_builder->function_stack.top();
        ir_builder->pushBlock(ir_condition_block);
        // 把ir_loop_before插入到条件块的开头
        ir_builder->insert(ir_loop_before);
        auto ir_condition = applyExpression(ast_while.condition);
        ir_builder->popBlock();

        auto ir_loop_block = ir::Block::create();
        ir_loop_block->parent_function = ir_builder->function_stack.top();
        auto ir_while = ir_builder->create<ir::While>(ir_condition, ir_condition_block,
                                                      ir_loop_block, ir_loop_before, ir_loop_after);

        ir_builder->pushBlock(ir_while->loopBlock());
        (*this)(ast_while.body);
        ir_builder->popBlock();

        ir_builder->loop_before_label_stack.pop();
        ir_builder->insert(ir_builder->loop_after_label_stack.top());
        ir_builder->loop_after_label_stack.pop();

        return ir_while;
    }

    Symbol operator()(ast::For ast_for) {
        auto ir_first = expression_lowering_visitor->apply(ast_for.first);
        auto ir_last = expression_lowering_visitor->apply(ast_for.last);
        if (ir_last->type != ir_builder->getIndexType() and
            !ir_builder->isArrayIndexType(ir_last->type)) {
            logger->error("the index type must be i64 or i64 array", ast_for.last);
        }
        if (ir_last->type != ir_first->type) {
            logger->error("the last firt type are not matched", ast_for.first);
        }

        auto ir_loop_before = ir::Label::create();
        ir_builder->loop_before_label_stack.push(ir_loop_before);
        auto ir_loop_after = ir::Label::create();
        ir_builder->loop_after_label_stack.push(ir_loop_after);

        // 这里使用了局部值拷贝的方式来模仿右值, 因为ir_first/last_value不会被改变
        auto ir_first_value = ir_builder->cloneValue(ir_first);
        auto ir_last_value = ir_builder->cloneValue(ir_last);
        auto ir_loop_block = ir::Block::create();
        ir_loop_block->parent_function = ir_builder->function_stack.top();
        auto ir_index = ir_builder->create<ir::LocalVariable>(ir_last->type);
        // 迭代变量应该在下一层迭代空间
        ir_builder->pushSymbolTable();
        ir_builder->setSymbolWithAssigningName(ir_index, ast_for.index);
        auto ir_for = ir_builder->create<ir::For>(ir_index, ir_first_value, ir_last_value,
                                                  ir_loop_block, ir_loop_before, ir_loop_after);

        ir_builder->pushBlock(ir_for->loopBlock());
        (*this)(ast_for.body);
        ir_builder->popBlock();

        ir_builder->popSymbolTable();

        for (auto ast_annotation : ast_for.annotation_dict) {
            std::list<std::string> values;
            std::transform(RANGE(ast_annotation.values), std::back_inserter(values),
                           [](ast::StringLiteral string_literal) { return string_literal.value; });
            ir_for->annotation_dict.insert({ast_annotation.name, values});
        }

        PRAJNA_ASSERT(not ir_for->loopBlock()->parent_block);

        ir_builder->loop_before_label_stack.pop();
        ir_builder->insert(ir_builder->loop_after_label_stack.top());
        ir_builder->loop_after_label_stack.pop();

        return ir_for;
    }

    Symbol operator()(ast::Break ast_break) {
        if (not ir_builder->loop_after_label_stack.size()) {
            logger->error("break is outside of a loop", ast_break);
        }

        return ir_builder->create<ir::JumpBranch>(ir_builder->loop_after_label_stack.top());
    }

    Symbol operator()(ast::Continue ast_continue) {
        if (not ir_builder->loop_before_label_stack.size()) {
            logger->error("continue is outside of a loop", ast_continue);
        }

        auto ir_before_label = ir_builder->loop_before_label_stack.top();
        if (auto ir_for =
                cast<ir::For>(ir_before_label->instruction_with_index_list.front().instruction)) {
            auto ir_i_and_1 = ir_builder->callBinaryOperator(ir_for->index(), "+",
                                                             {ir_builder->getIndexConstant(1)});
            ir_builder->create<ir::WriteVariableLiked>(ir_i_and_1, ir_for->index());
            return ir_builder->create<ir::JumpBranch>(ir_builder->loop_before_label_stack.top());
        } else {
            return ir_builder->create<ir::JumpBranch>(ir_builder->loop_before_label_stack.top());
        }
    }

    void createStructConstructor(std::shared_ptr<ir::StructType> ir_struct_type) {
        auto ir_fields = ir_struct_type->fields;
        std::list<std::shared_ptr<ir::Type>> ir_constructor_arg_types(ir_fields.size());
        std::transform(RANGE(ir_fields), ir_constructor_arg_types.begin(),
                       [](auto ir_field) { return ir_field->type; });
        auto ir_constructor_type =
            ir::FunctionType::create(ir_constructor_arg_types, ir_struct_type);
        auto ir_constructor = ir::Function::create(ir_constructor_type);
        ir_constructor_type->function = ir_constructor;
        ir_struct_type->constructor = ir_constructor;
        ir_constructor->name = concatFullname(ir_struct_type->name, "constructor");
        ir_constructor->fullname = ir_constructor->name;
        ir_constructor->parent_module = ir_builder->module;
        ir_builder->module->functions.push_back(ir_constructor);

        ir_builder->createTopBlockForFunction(ir_constructor);

        auto ir_variable = ir_builder->create<ir::LocalVariable>(ir_struct_type);
        for (auto [ir_field, ir_parameter] :
             boost::combine(ir_fields, ir_constructor->parameters)) {
            auto ir_access_field = ir_builder->create<ir::AccessField>(ir_variable, ir_field);
            ir_builder->create<ir::WriteVariableLiked>(ir_parameter, ir_access_field);
        }
        ir_builder->create<ir::Return>(ir_variable);
        ir_builder->popBlock();
        ir_builder->function_stack.pop();
    }

    std::shared_ptr<ir::Type> applyType(ast::Type ast_postfix_type) {
        return expression_lowering_visitor->applyType(ast_postfix_type);
    }

    Symbol operator()(ast::Struct ast_struct) {
        auto ir_struct_type = ir::StructType::create();
        ir_builder->instantiating_type_stack.push(ir_struct_type);

        // TODO 名字重命名错误, 需要处理
        ir_builder->symbol_table->setWithAssigningName(
            ir_struct_type,
            expression_lowering_visitor->getNameOfTemplateIdentfier(ast_struct.name));

        std::list<std::shared_ptr<ir::Field>> ir_fields;
        std::transform(
            RANGE(ast_struct.fields), std::back_inserter(ir_fields), [=](ast::Field ast_field) {
                return ir::Field::create(ast_field.name, this->applyType(ast_field.type));
            });
        ir_struct_type->fields = ir_fields;
        ir_struct_type->update();
        this->createStructConstructor(ir_struct_type);

        ir_builder->instantiating_type_stack.pop();

        return ir_struct_type;
    }

    void processInitializeCopyDestroyFunction(std::shared_ptr<ir::Type> ir_type,
                                              std::shared_ptr<ir::Function> ir_function,
                                              ast::Function ast_function) {
        // 处理copy等回调函数
        std::set<std::string> copy_destroy_callback_names = {"initialize", "copy", "destroy"};
        if (not copy_destroy_callback_names.count(ir_function->name)) return;

        if (ir_function->annotation_dict.count("static") != 0) {
            auto iter_static_annotation = std::find_if(
                RANGE(ast_function.declaration.annotation_dict),
                [](ast::Annotation ast_annotation) { return ast_annotation.name == "static"; });
            logger->error(fmt::format("the {} function can not be static", ir_function->name),
                          *iter_static_annotation);
        }

        if (not is<ir::VoidType>(ir_function->function_type->return_type)) {
            logger->error(
                fmt::format("the {} function return type must be void", ir_function->name),
                *ast_function.declaration.return_type);
        }
        // this pointer argument
        if (ir_function->function_type->parameter_types.size() != 1) {
            logger->error(
                fmt::format("the {} function should has no parameters", ir_function->name),
                ast_function.declaration.parameters);
        }
    }

    Symbol operator()(ast::ImplementType ast_implement) {
        try {
            auto ir_type = expression_lowering_visitor->applyType(ast_implement.type);

            ir_builder->pushSymbolTable();
            ir_builder->symbol_table->name = ir_type->name;

            PRAJNA_ASSERT(!ir_builder->this_pointer_type);
            ir_builder->this_pointer_type = ir::PointerType::create(ir_type);

            for (auto ast_statement : ast_implement.statements) {
                auto symbol_function = (*this)(ast_statement);
                if (auto ir_function = cast<ir::Function>(symbolGet<ir::Value>(symbol_function))) {
                    ir_type->function_dict[ir_function->name] = ir_function;
                    continue;
                }
                if (auto lowering_template = symbolGet<Template>(symbol_function)) {
                    ir_type->template_any_dict[lowering_template->name] = lowering_template;
                    continue;
                }
                PRAJNA_TODO;
            }

            ir_builder->popSymbolTable();
            ir_builder->this_pointer_type = nullptr;
        } catch (CompileError compile_error) {
            logger->note(ast_implement.type);
            throw compile_error;
        }

        return nullptr;
    }

    Symbol operator()(ast::ImplementInterfaceForType ast_implement) {
        try {
            auto ir_type = expression_lowering_visitor->applyType(ast_implement.type);

            ir_builder->pushSymbolTable();
            ir_builder->symbol_table->name = ir_type->name;
            PRAJNA_ASSERT(!ir_builder->this_pointer_type);
            ir_builder->this_pointer_type = ir::PointerType::create(ir_type);

            auto ir_interface = ir::InterfaceImplement::create();
            auto ir_interface_prototype = symbolGet<ir::InterfacePrototype>(
                expression_lowering_visitor->applyIdentifierPath(ast_implement.interface));
            if (!ir_interface_prototype) {
                logger->error("not invalid interface", ast_implement.interface);
            }
            ir_interface->name = ir_interface_prototype->name;
            ir_interface->prototype = ir_interface_prototype;
            ir_interface->fullname = concatFullname(ir_type->fullname, ir_interface->name);
            if (ir_type->interface_dict[ir_interface->name]) {
                logger->error("interface has implemented", ast_implement.interface);
            }
            ir_type->interface_dict[ir_interface->name] = ir_interface;

            ir_builder->pushSymbolTable();
            ir_builder->symbol_table->name = ir_interface->name;

            // 声明成员函数
            for (auto ast_function : ast_implement.functions) {
                for (auto ast_annotation : ast_function.declaration.annotation_dict) {
                    if (ast_annotation.name == "static") {
                        logger->error("static function is invalid", ast_annotation);
                    }
                }

                auto ir_function = this->applyFunctionHeader(ast_function.declaration);
                ir_interface->functions.push_back(ir_function);
                ir_function->is_declaration = true;
            }

            // 需要前置, 因为函数会用的接口类型本身
            if (!ir_interface_prototype->disable_dynamic_dispatch) {
                // 创建接口动态类型生成函数
                ir_interface->dynamic_type_creator = ir_builder->createFunction(
                    "dynamic_type_creator",
                    ir::FunctionType::create({ir_builder->getPtrType(ir_type)},
                                             ir_interface->prototype->dynamic_type));
            }

            for (auto ast_function : ast_implement.functions) {
                std::shared_ptr<ir::Function> ir_function = nullptr;
                auto symbol_function = (*this)(ast_function);
                ir_function = cast<ir::Function>(symbolGet<ir::Value>(symbol_function));
            }

            if (ir_interface_prototype->disable_dynamic_dispatch) {
                ir_builder->popSymbolTable();
                ir_builder->this_pointer_type = nullptr;
                ir_builder->popSymbolTable();
                return nullptr;
            }

            // 包装一个undef this pointer的函数
            for (auto ir_function : ir_interface->functions) {
                std::list<std::shared_ptr<ir::Type>> ir_undef_this_pointer_function_argument_types =
                    ir_function->function_type->parameter_types;
                ir_undef_this_pointer_function_argument_types.front() =
                    ir::PointerType::create(ir::UndefType::create());

                auto ir_undef_this_pointer_function_type =
                    ir::FunctionType::create(ir_undef_this_pointer_function_argument_types,
                                             ir_function->function_type->return_type);
                auto ir_undef_this_pointer_function = ir_builder->createFunction(
                    ir_function->name + "/undef", ir_undef_this_pointer_function_type);

                ir_interface->undef_this_pointer_functions.insert(
                    {ir_function, ir_undef_this_pointer_function});

                ir_undef_this_pointer_function->blocks.push_back(ir::Block::create());
                ir_undef_this_pointer_function->blocks.back()->parent_function =
                    ir_undef_this_pointer_function;
                ir_builder->pushBlock(ir_undef_this_pointer_function->blocks.back());
                // 将undef *的this pointer转为本身的指针类型
                auto ir_this_pointer = ir_builder->create<ir::BitCast>(
                    ir_undef_this_pointer_function->parameters.front(),
                    ir_function->parameters.front()->type);
                auto ir_arguments = ir_undef_this_pointer_function->parameters;
                ir_arguments.front() = ir_this_pointer;
                ir_builder->create<ir::Return>(
                    ir_builder->create<ir::Call>(ir_function, ir_arguments));
                ir_builder->popBlock();
            }

            ir_interface->dynamic_type_creator->blocks.push_back(ir::Block::create());
            ir_interface->dynamic_type_creator->blocks.back()->parent_function =
                ir_interface->dynamic_type_creator;
            ir_builder->pushBlock(ir_interface->dynamic_type_creator->blocks.back());

            auto ir_self =
                ir_builder->create<ir::LocalVariable>(ir_interface->prototype->dynamic_type);

            ir_builder->create<ir::WriteVariableLiked>(
                ir_builder->callMemberFunction(
                    ir_interface->dynamic_type_creator->parameters.front(), "toUndef", {}),
                ir_builder->accessField(ir_self, "object_pointer"));
            for (auto ir_function : ir_interface->functions) {
                auto iter_field = std::find_if(
                    RANGE(ir_interface->prototype->dynamic_type->fields),
                    [=](auto ir_field) { return ir_field->name == ir_function->name + "/fp"; });
                PRAJNA_ASSERT(iter_field != ir_interface->prototype->dynamic_type->fields.end());
                auto ir_function_pointer =
                    ir_builder->create<ir::AccessField>(ir_self, *iter_field);
                ir_builder->create<ir::WriteVariableLiked>(
                    ir_interface->undef_this_pointer_functions[ir_function], ir_function_pointer);
            }
            ir_builder->create<ir::Return>(ir_self);
            ir_builder->popBlock();

            ir_builder->popSymbolTable();
            ir_builder->this_pointer_type = nullptr;
            ir_builder->popSymbolTable();
        } catch (CompileError compile_error) {
            logger->note(ast_implement.type);
            throw compile_error;
        }

        return nullptr;
    }

    std::list<Symbol> getTemplateConcepts(ast::TemplateParameters ast_template_parameter_list) {
        std::list<Symbol> symbol_template_concepts;
        std::transform(RANGE(ast_template_parameter_list),
                       std::back_inserter(symbol_template_concepts),
                       [=](ast::TemplateParameter ast_template_parameter) -> Symbol {
                           if (ast_template_parameter.concept_) {
                               return expression_lowering_visitor->applyIdentifierPath(
                                   *ast_template_parameter.concept_);
                           } else {
                               return nullptr;
                           }
                       });
        return symbol_template_concepts;
    }

    std::shared_ptr<Template> getOrCreateTemplate(ast::Identifier ast_identifier) {
        if (ir_builder->symbol_table->currentTableHas(ast_identifier)) {
            auto template_ = symbolGet<Template>(ir_builder->symbol_table->get(ast_identifier));
            PRAJNA_ASSERT(template_);
            return template_;
        } else {
            auto template_ = Template::create();
            ir_builder->setSymbolWithAssigningName(template_, ast_identifier);
            return template_;
        }
    }

    Symbol operator()(ast::Template ast_template) {
        auto template_ = this->getOrCreateTemplate(ast_template.name);
        template_->generator = this->createTemplateGenerator(ast_template.template_parameters,
                                                             ast_template.statements);
        return template_;
    }

    Symbol operator()(ast::TemplateInstance ast_template_instance) {
        try {
            expression_lowering_visitor->applyIdentifierPath(ast_template_instance.identifier_path);
            return nullptr;
        } catch (CompileError compile_error) {
            logger->note(ast_template_instance);
            throw compile_error;
        }
    }

    std::list<std::string> getTemplateParametersIdentifiers(
        std::list<ast::TemplateParameter> template_parameters) {
        std::set<ast::Identifier> template_parameter_identifier_set;
        std::list<std::string> template_parameter_identifier_list;
        for (auto template_parameter : template_parameters) {
            if (template_parameter_identifier_set.count(template_parameter.name)) {
                logger->error("the template parameter is defined already", template_parameter);
            }
            template_parameter_identifier_list.push_back(template_parameter.name);
            template_parameter_identifier_set.insert(template_parameter.name);
        }

        return template_parameter_identifier_list;
    }

    Template::Generator createTemplateGenerator(ast::TemplateParameters ast_template_paramters,
                                                ast::Statement ast_statement) {
        auto template_parameter_identifier_list =
            this->getTemplateParametersIdentifiers(ast_template_paramters);
        return [=, symbol_table = ir_builder->symbol_table, logger = this->logger](
                   std::list<Symbol> symbol_template_arguments,
                   std::shared_ptr<ir::Module> ir_module) -> Symbol {
            // 包裹一层名字空间, 避免被污染
            auto templates_symbol_table = SymbolTable::create(symbol_table);

            for (auto [identifier, symbol] :
                 boost::combine(template_parameter_identifier_list, symbol_template_arguments)) {
                templates_symbol_table->set(symbol, identifier);
            }

            auto statement_lowering_visitor = StatementLoweringVisitor::create(
                templates_symbol_table, logger, ir_module, nullptr);
            try {
                return (*statement_lowering_visitor)(ast_statement);
            } catch (CompileError compile_error) {
                logger->note(ast_template_paramters);
                throw compile_error;
            }
        };
    }

    Symbol operator()(ast::TemplateStatement ast_template_statement) {
        auto template_concepts =
            this->getTemplateConcepts(ast_template_statement.template_parameters);

        return boost::apply_visitor(
            overloaded{
                [=](ast::Struct ast_struct) -> Symbol {
                    std::shared_ptr<TemplateStruct> template_struct;
                    if (ir_builder->symbol_table->currentTableHas(ast_struct.name.identifier)) {
                        template_struct = symbolGet<TemplateStruct>(
                            ir_builder->symbol_table->get(ast_struct.name.identifier));
                        PRAJNA_ASSERT(template_struct);
                    } else {
                        template_struct = TemplateStruct::create();
                        ir_builder->setSymbolWithAssigningName(template_struct,
                                                               ast_struct.name.identifier);
                    }

                    template_struct->template_struct_impl->generator =
                        this->createTemplateGenerator(ast_template_statement.template_parameters,
                                                      ast_struct);

                    return template_struct;
                },
                [=](ast::ImplementInterfaceForType ast_implement_type_for_interface) -> Symbol {
                    auto template_parameter_identifier_list = getTemplateParametersIdentifiers(
                        ast_template_statement.template_parameters);

                    if (ast_implement_type_for_interface.type.postfix_type_operators.size()) {
                        // 只能是原始的type, 因为是自动特化的
                        logger->error("invalid template implement",
                                      ast_implement_type_for_interface.type);
                    }
                    // TODO
                    PRAJNA_ASSERT(ast_implement_type_for_interface.type.base_type.which() == 1);
                    auto ast_identifier_path = boost::get<ast::IdentifierPath>(
                        ast_implement_type_for_interface.type.base_type);
                    // 只获取模板类, 不能带模板参数
                    auto ast_template_struct = ast_identifier_path;
                    ast_template_struct.identifiers.back().template_arguments = boost::none;
                    auto ir_symbol =
                        expression_lowering_visitor->applyIdentifierPath(ast_template_struct);
                    auto template_struct = symbolGet<TemplateStruct>(ir_symbol);
                    if (template_struct == nullptr) {
                        logger->error("it's not a template struct but with template parameters",
                                      ast_identifier_path);
                    }

                    auto template_ = Template::create();
                    template_->generator =
                        this->createTemplateGenerator(ast_template_statement.template_parameters,
                                                      ast_implement_type_for_interface);
                    template_struct->template_implement_type_vec.push_back(template_);

                    return template_;
                },
                [=](ast::ImplementType ast_implement_type) -> Symbol {
                    auto template_parameter_identifier_list = getTemplateParametersIdentifiers(
                        ast_template_statement.template_parameters);

                    if (ast_implement_type.type.postfix_type_operators.size()) {
                        // 只能是原始的type, 因为是自动特化的
                        logger->error("invalid template implement", ast_implement_type.type);
                    }
                    // TODO
                    PRAJNA_ASSERT(ast_implement_type.type.base_type.which() == 1);
                    auto ast_identifier_path =
                        boost::get<ast::IdentifierPath>(ast_implement_type.type.base_type);
                    // 只获取模板类, 不能带模板参数
                    auto ast_template_struct = ast_identifier_path;
                    ast_template_struct.identifiers.back().template_arguments = boost::none;
                    auto ir_symbol =
                        expression_lowering_visitor->applyIdentifierPath(ast_template_struct);
                    auto template_struct = symbolGet<TemplateStruct>(ir_symbol);
                    if (template_struct == nullptr) {
                        logger->error("it's not a template struct but with template parameters",
                                      ast_identifier_path);
                    }

                    auto template_ = Template::create();
                    template_->generator = this->createTemplateGenerator(
                        ast_template_statement.template_parameters, ast_implement_type);
                    template_struct->template_implement_type_vec.push_back(template_);
                    return template_;
                },
                [=](ast::InterfacePrototype ast_interface_prototype) -> Symbol {
                    auto template_ =
                        this->getOrCreateTemplate(ast_interface_prototype.name.identifier);
                    template_->generator = this->createTemplateGenerator(
                        ast_template_statement.template_parameters, ast_interface_prototype);
                    return template_;
                },
                [=](ast::Function ast_function) -> Symbol {
                    auto ast_template_paramters = ast_template_statement.template_parameters;
                    auto template_ = this->getOrCreateTemplate(ast_function.declaration.name);

                    if (ir_builder->isInsideImplement()) {
                        auto template_parameter_identifier_list =
                            this->getTemplateParametersIdentifiers(ast_template_paramters);
                        template_->generator =
                            [=, symbol_table = ir_builder->symbol_table, logger = this->logger,
                             this_pointer_type = ir_builder->this_pointer_type](
                                std::list<Symbol> symbol_template_arguments,
                                std::shared_ptr<ir::Module> ir_module) -> Symbol {
                            // 包裹一层名字空间, 避免被污染
                            auto templates_symbol_table = SymbolTable::create(symbol_table);
                            templates_symbol_table->name =
                                getSymbolListFullname(symbol_template_arguments);

                            for (auto [identifier, symbol] :
                                 boost::combine(template_parameter_identifier_list,
                                                symbol_template_arguments)) {
                                templates_symbol_table->set(symbol, identifier);
                            }

                            auto statement_lowering_visitor = StatementLoweringVisitor::create(
                                templates_symbol_table, logger, ir_module, nullptr);
                            try {
                                statement_lowering_visitor->ir_builder->this_pointer_type =
                                    this_pointer_type;
                                statement_lowering_visitor->ir_builder
                                    ->symbol_template_argument_list = symbol_template_arguments;

                                auto symbol = (*statement_lowering_visitor)(ast_function);

                                statement_lowering_visitor->ir_builder->this_pointer_type = nullptr;
                                statement_lowering_visitor->ir_builder
                                    ->symbol_template_argument_list.clear();

                                statement_lowering_visitor->ir_builder->this_pointer_type = nullptr;
                                return symbol;
                            } catch (CompileError compile_error) {
                                logger->note(ast_template_paramters);
                                throw compile_error;
                            }
                        };
                    } else {
                        template_->generator = this->createTemplateGenerator(
                            ast_template_statement.template_parameters, ast_function);
                    }
                    return template_;
                }},
            ast_template_statement.statement);
    }

    Symbol operator()(ast::SpecialStatement ast_special_statement) {
        PRAJNA_TODO;
        return nullptr;
    }

    Symbol operator()(ast::Expression ast_expression) {
        return this->applyExpression(ast_expression);
    }

    Symbol operator()(ast::Import ast_import);

    Symbol operator()(ast::Export ast_export) {
        auto symbol = ir_builder->symbol_table->get(ast_export.identifier);
        if (symbol.which() == 0) {
            logger->error("the symbol is not found", ast_export.identifier);
        }

        auto parent_symbol_table = ir_builder->symbol_table->parent_symbol_table;
        PRAJNA_ASSERT(parent_symbol_table, "不可能为nullptr, 因为文件名本身也是一层名字空间");

        if (parent_symbol_table->currentTableHas(ast_export.identifier)) {
            logger->error("the identifier is exist already", ast_export.identifier);
        }

        parent_symbol_table->set(symbol, ast_export.identifier);

        return symbol;
    }

    std::shared_ptr<Template> createCastInstructionTemplate() {
        auto template_cast_instruction = Template::create();
        template_cast_instruction->generator =
            [symbol_table = this->ir_builder->symbol_table, logger = this->logger, this](
                std::list<Symbol> symbol_template_arguments,
                std::shared_ptr<ir::Module> ir_module) -> Symbol {
            // TODO
            PRAJNA_ASSERT(symbol_template_arguments.size() == 2);
            auto ir_source_type = symbolGet<ir::Type>(symbol_template_arguments.front());
            auto ir_target_type = symbolGet<ir::Type>(symbol_template_arguments.back());
            auto ir_tmp_builder = IrBuilder::create(symbol_table, ir_module, logger);
            auto ir_function_type = ir::FunctionType::create({ir_source_type}, ir_target_type);
            auto ir_function = ir_tmp_builder->createFunction(
                "__cast" + getSymbolListFullname(symbol_template_arguments), ir_function_type);
            ir_tmp_builder->createTopBlockForFunction(ir_function);

            if (ir_source_type == ir_target_type) {
                ir_tmp_builder->create<ir::Return>(ir_function->parameters.front());
            } else {
                auto cast_operation = ir::CastInstruction::Operation::BitCast;

                if (auto ir_source_int_type = cast<ir::IntType>(ir_source_type)) {
                    if (is<ir::FloatType>(ir_target_type)) {
                        cast_operation = ir_source_int_type->is_signed
                                             ? ir::CastInstruction::Operation::SIToFP
                                             : ir::CastInstruction::Operation::UIToFP;
                    }
                    if (auto ir_target_int_type = cast<ir::IntType>(ir_target_type)) {
                        if (ir_source_int_type->bits > ir_target_int_type->bits) {
                            cast_operation = ir::CastInstruction::Operation::Trunc;
                        }
                        if (ir_source_int_type->bits < ir_target_int_type->bits) {
                            cast_operation = ir_target_int_type->is_signed
                                                 ? ir::CastInstruction::Operation::SExt
                                                 : ir::CastInstruction::Operation::ZExt;
                        }
                    }
                    if (is<ir::PointerType>(ir_target_type)) {
                        cast_operation = ir::CastInstruction::Operation::IntToPtr;
                    }
                }

                if (auto ir_source_float_type = cast<ir::FloatType>(ir_source_type)) {
                    if (auto ir_target_float_type = cast<ir::FloatType>(ir_target_type)) {
                        cast_operation = ir_source_float_type->bits > ir_target_float_type->bits
                                             ? ir::CastInstruction::Operation::FPTrunc
                                             : ir::CastInstruction::Operation::FPExt;
                    }
                    if (auto ir_target_int_type = cast<ir::IntType>(ir_target_type)) {
                        cast_operation = ir_target_int_type->is_signed
                                             ? ir::CastInstruction::Operation::FPToSI
                                             : ir::CastInstruction::Operation::FPToUI;
                    }
                }

                if (is<ir::PointerType>(ir_source_type)) {
                    if (is<ir::IntType>(ir_target_type)) {
                        cast_operation = ir::CastInstruction::Operation::PtrToInt;
                    }
                }

                PRAJNA_ASSERT(cast_operation != ir::CastInstruction::Operation::BitCast);
                ir_tmp_builder->create<ir::Return>(ir_tmp_builder->create<ir::CastInstruction>(
                    cast_operation, ir_function->parameters.front(), ir_target_type));
            }
            return ir_function;
        };

        return template_cast_instruction;
    }

    std::shared_ptr<TemplateStruct> createIntTypeTemplate(bool is_signed) {
        auto template_int = Template::create();

        template_int->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                   logger = this->logger](
                                      std::list<Symbol> symbol_template_arguments,
                                      std::shared_ptr<ir::Module> ir_module) -> Symbol {
            // TODO
            PRAJNA_ASSERT(symbol_template_arguments.size() == 1);

            auto ir_constant_bit_size =
                symbolGet<ir::ConstantInt>(symbol_template_arguments.front());

            return ir::IntType::create(ir_constant_bit_size->value, is_signed);
        };

        auto template_struct_int = TemplateStruct::create();
        template_struct_int->template_struct_impl = template_int;

        return template_struct_int;
    }

    std::shared_ptr<TemplateStruct> createFloatTypeTemplate() {
        auto template_int = Template::create();

        template_int->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                   logger = this->logger](
                                      std::list<Symbol> symbol_template_arguments,
                                      std::shared_ptr<ir::Module> ir_module) -> Symbol {
            // TODO
            PRAJNA_ASSERT(symbol_template_arguments.size() == 1);

            auto ir_constant_bit_size =
                symbolGet<ir::ConstantInt>(symbol_template_arguments.front());
            // TODO, 还需要再处理下, LLVM支持的float类型有限.
            PRAJNA_ASSERT(ir_constant_bit_size->value % 4 == 0);

            return ir::FloatType::create(ir_constant_bit_size->value);
        };

        auto template_struct_int = TemplateStruct::create();
        template_struct_int->template_struct_impl = template_int;

        return template_struct_int;
    }

    std::shared_ptr<Template> createBitCastTemplate() {
        auto lowering_template = Template::create();

        lowering_template->generator = [symbol_table = this->ir_builder->symbol_table,
                                        logger = this->logger,
                                        this](std::list<Symbol> symbol_template_arguments,
                                              std::shared_ptr<ir::Module> ir_module) -> Symbol {
            // TODO
            PRAJNA_ASSERT(symbol_template_arguments.size() == 2);

            auto ir_source_type = symbolGet<ir::Type>(symbol_template_arguments.front());
            auto ir_target_type = symbolGet<ir::Type>(symbol_template_arguments.back());
            PRAJNA_ASSERT(ir_source_type->bytes == ir_target_type->bytes);
            auto ir_tmp_builder = IrBuilder::create(symbol_table, ir_module, logger);
            auto ir_function_type = ir::FunctionType::create({ir_source_type}, ir_target_type);
            auto ir_function = ir_tmp_builder->createFunction(
                "__bit_cast<" + ir_source_type->fullname + ", " + ir_target_type->fullname + ">",
                ir_function_type);
            ir_tmp_builder->createTopBlockForFunction(ir_function);
            ir_tmp_builder->create<ir::Return>(ir_tmp_builder->create<ir::BitCast>(
                ir_function->parameters.front(), ir_target_type));
            return ir_function;
        };

        return lowering_template;
    }

    std::shared_ptr<Template> createCompareInstructionTemplate(std::string compare_operation_name) {
        auto template_compare = Template::create();

        template_compare->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                       logger = this->logger](
                                          std::list<Symbol> symbol_template_arguments,
                                          std::shared_ptr<ir::Module> ir_module) -> Symbol {
            // TODO
            PRAJNA_ASSERT(symbol_template_arguments.size() == 1);
            auto ir_type = symbolGet<ir::Type>(symbol_template_arguments.front());

            auto compare_operation = ir::CompareInstruction::Operation::None;
            // arithmatic
            if (compare_operation_name == "eq") {
                if (is<ir::IntType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::ICMP_EQ;
                } else {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_OEQ;
                }
            }
            if (compare_operation_name == "ne") {
                if (is<ir::IntType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::ICMP_NE;
                } else {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_ONE;
                }
            }
            if (compare_operation_name == "gt") {
                if (auto ir_int_type = cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_SGT;
                    } else {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_UGT;
                    }
                }
                if (is<ir::FloatType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_OGT;
                }
            }
            if (compare_operation_name == "ge") {
                if (auto ir_int_type = cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_SGE;
                    } else {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_UGE;
                    }
                }
                if (is<ir::FloatType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_OGE;
                }
            }
            if (compare_operation_name == "lt") {
                if (auto ir_int_type = cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_SLT;
                    } else {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_ULT;
                    }
                }
                if (is<ir::FloatType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_OLT;
                }
            }
            if (compare_operation_name == "le") {
                if (auto ir_int_type = cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_SLE;
                    } else {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_ULE;
                    }
                }
                if (is<ir::FloatType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_OLE;
                }
            }

            PRAJNA_ASSERT(compare_operation != ir::CompareInstruction::Operation::None);

            auto ir_tmp_builder = IrBuilder::create(symbol_table, ir_module, logger);
            auto ir_function_type =
                ir::FunctionType::create({ir_type, ir_type}, ir::BoolType::create());
            auto ir_function = ir_tmp_builder->createFunction(
                "__" + compare_operation_name + getSymbolListFullname(symbol_template_arguments),
                ir_function_type);
            ir_tmp_builder->createTopBlockForFunction(ir_function);
            ir_tmp_builder->create<ir::Return>(ir_tmp_builder->create<ir::CompareInstruction>(
                compare_operation, ir_function->parameters.front(),
                ir_function->parameters.back()));
            return ir_function;
        };

        return template_compare;
    }

    // TODO 类型不对应, 会导致错误.
    std::shared_ptr<Template> createBinaryOperatorTemplate(std::string binary_operator_name) {
        auto template_binary_operator = Template::create();

        template_binary_operator->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                               logger = this->logger](
                                                  std::list<Symbol> symbol_template_arguments,
                                                  std::shared_ptr<ir::Module> ir_module) -> Symbol {
            // TODO
            PRAJNA_ASSERT(symbol_template_arguments.size() == 1);
            auto ir_type = symbolGet<ir::Type>(symbol_template_arguments.front());

            auto binary_operation = ir::BinaryOperator::Operation::None;
            // arithmatic
            if (binary_operator_name == "add") {
                if (is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Add;
                }
                if (is<ir::FloatType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::FAdd;
                }
            }
            if (binary_operator_name == "sub") {
                if (is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Sub;
                }
                if (is<ir::FloatType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::FSub;
                }
            }
            if (binary_operator_name == "mul") {
                if (is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Mul;
                }
                if (is<ir::FloatType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::FMul;
                }
            }
            if (binary_operator_name == "div") {
                if (auto ir_int_type = cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        binary_operation = ir::BinaryOperator::Operation::SDiv;
                    } else {
                        binary_operation = ir::BinaryOperator::Operation::UDiv;
                    }
                }
                if (is<ir::FloatType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::FDiv;
                }
            }
            if (binary_operator_name == "rem") {
                if (auto ir_int_type = cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        binary_operation = ir::BinaryOperator::Operation::SRem;
                    } else {
                        binary_operation = ir::BinaryOperator::Operation::URem;
                    }
                }
                if (is<ir::FloatType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::FRem;
                }
            }
            // logical
            if (binary_operator_name == "and") {
                if (is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::And;
                }
            }
            if (binary_operator_name == "or") {
                if (is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Or;
                }
            }
            if (binary_operator_name == "xor") {
                if (is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Xor;
                }
            }

            // shift
            if (binary_operator_name == "shift_left") {
                if (is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Shl;
                }
            }
            if (binary_operator_name == "shift_right") {
                if (auto ir_int_type = cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        binary_operation = ir::BinaryOperator::Operation::AShr;
                    } else {
                        binary_operation = ir::BinaryOperator::Operation::LShr;
                    }
                }
            }

            PRAJNA_ASSERT(binary_operation != ir::BinaryOperator::Operation::None);

            auto ir_tmp_builder = IrBuilder::create(symbol_table, ir_module, logger);
            auto ir_function_type = ir::FunctionType::create({ir_type, ir_type}, ir_type);
            auto ir_function = ir_tmp_builder->createFunction(
                "__" + binary_operator_name + getSymbolListFullname(symbol_template_arguments),
                ir_function_type);
            ir_tmp_builder->createTopBlockForFunction(ir_function);
            ir_tmp_builder->create<ir::Return>(ir_tmp_builder->create<ir::BinaryOperator>(
                binary_operation, ir_function->parameters.front(), ir_function->parameters.back()));
            return ir_function;
        };

        return template_binary_operator;
    }

    Symbol operator()(ast::Pragma ast_pragma) {
        if (ast_pragma.name == "error") {
            std::string msg = ast_pragma.values.size() ? ast_pragma.values.front().value : "";
            logger->error(fmt::format("pragma error: {}", msg), ast_pragma);
            return nullptr;
        }
        if (ast_pragma.name == "warning") {
            std::string msg = ast_pragma.values.size() ? ast_pragma.values.front().value : "";
            logger->warning(fmt::format("pragma error: {}", msg), ast_pragma);
            return nullptr;
        }
        if (ast_pragma.name == "system") {
            std::string command = ast_pragma.values.size() ? ast_pragma.values.front().value : "";
            std::system(command.c_str());
            return nullptr;
        }
        if (ast_pragma.name == "stage1") {
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBitCastTemplate(), "__bit_cast");

            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createIntTypeTemplate(true), "int");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createIntTypeTemplate(false), "uint");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createFloatTypeTemplate(), "float");

            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createCastInstructionTemplate(), "__cast");
            // arithematic
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBinaryOperatorTemplate("add"), "__add");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBinaryOperatorTemplate("sub"), "__sub");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBinaryOperatorTemplate("mul"), "__mul");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBinaryOperatorTemplate("div"), "__div");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBinaryOperatorTemplate("rem"), "__rem");
            // logical
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBinaryOperatorTemplate("and"), "__and");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBinaryOperatorTemplate("or"), "__or");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBinaryOperatorTemplate("xor"), "__xor");
            // shift
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBinaryOperatorTemplate("shift_left"), "__shift_left");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createBinaryOperatorTemplate("shift_right"), "__shift_right");

            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createCompareInstructionTemplate("eq"), "__eq");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createCompareInstructionTemplate("ne"), "__ne");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createCompareInstructionTemplate("gt"), "__gt");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createCompareInstructionTemplate("ge"), "__ge");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createCompareInstructionTemplate("lt"), "__lt");
            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                this->createCompareInstructionTemplate("le"), "__le");

            ir_builder->symbol_table->rootSymbolTable()->setWithAssigningName(
                expression_lowering_visitor->createDynamicTemplate(), "dynamic");
            return nullptr;
        }

        logger->error("the pragma is undefined", ast_pragma);
        return nullptr;
    }

    Symbol operator()(ast::InterfacePrototype ast_interface_prototype) {
        auto ir_interface_prototype = ir::InterfacePrototype::create();
        auto ir_interface_prototype_name =
            expression_lowering_visitor->getNameOfTemplateIdentfier(ast_interface_prototype.name);
        ir_builder->symbol_table->setWithAssigningName(ir_interface_prototype,
                                                       ir_interface_prototype_name);

        ir_interface_prototype->disable_dynamic_dispatch = std::any_of(
            RANGE(ast_interface_prototype.annotation_dict),
            [](auto ast_annotation) { return ast_annotation.name == "disable_dynamic_dispatch"; });

        if (!ir_interface_prototype->disable_dynamic_dispatch) {
            ir_interface_prototype->dynamic_type = ir::StructType::create({});
        }

        for (auto ast_function_declaration : ast_interface_prototype.functions) {
            ///  TODO function_type and name , 下面的代码会导致函数重复
            ir_builder->pushSymbolTable();
            ir_builder->symbol_table->name = ir_interface_prototype_name;
            auto symbol_function = (*this)(ast_function_declaration);
            auto ir_function = cast<ir::Function>(symbolGet<ir::Value>(symbol_function));
            ir_builder->popSymbolTable();
            ir_interface_prototype->functions.push_back(ir_function);
            // 需要将去从module移出来, 这里的function并不会实际被生成
            ir_builder->module->functions.remove(ir_function);
        }

        if (!ir_interface_prototype->disable_dynamic_dispatch) {
            this->createInterfaceDynamicType(ir_interface_prototype);
        }

        // 用到的时候再进行该操作, 因为很多原生接口实现时候ptr还未

        return ir_interface_prototype;
    }

    void createInterfaceDynamicType(
        std::shared_ptr<ir::InterfacePrototype> ir_interface_prototype) {
        auto field_object_pointer =
            ir::Field::create("object_pointer", ir_builder->getPtrType(ir::UndefType::create()));
        auto ir_interface_struct = ir_interface_prototype->dynamic_type;
        ir_interface_struct->fields.push_back(field_object_pointer);
        ir_interface_struct->name = ir_interface_prototype->name;
        ir_interface_struct->fullname = ir_interface_prototype->fullname;

        PRAJNA_ASSERT(!ir_builder->this_pointer_type);
        ir_builder->this_pointer_type = ir::PointerType::create(ir_interface_struct);

        auto ir_interface = ir::InterfaceImplement::create();
        for (auto ir_function : ir_interface_prototype->functions) {
            auto ir_callee_argument_types = ir_function->function_type->parameter_types;
            ir_callee_argument_types.insert(ir_callee_argument_types.begin(),
                                            ir::PointerType::create(ir::UndefType::create()));
            auto ir_callee_type = ir::FunctionType::create(ir_callee_argument_types,
                                                           ir_function->function_type->return_type);

            auto field_function_pointer = ir::Field::create(
                ir_function->name + "/fp", ir::PointerType::create(ir_callee_type));
            ir_interface_struct->fields.push_back(field_function_pointer);
            ir_interface_struct->update();

            auto ir_member_function_argument_types = ir_function->function_type->parameter_types;
            // 必然有一个this pointer参数
            // PRAJNA_ASSERT(ir_member_function_argument_types.size() >= 1);
            // 第一个参数应该是this pointer的类型, 修改一下
            ir_member_function_argument_types.insert(ir_member_function_argument_types.begin(),
                                                     ir_builder->this_pointer_type);
            auto ir_member_function_type = ir::FunctionType::create(
                ir_member_function_argument_types, ir_function->function_type->return_type);
            auto ir_member_function =
                ir_builder->createFunction(ir_function->name + "/member", ir_member_function_type);
            ir_member_function->name = ir_function->name;
            ir_member_function->fullname = ir_function->fullname;

            // 这里还是使用类型IrBuilder
            ir_member_function->blocks.push_back(ir::Block::create());
            ir_member_function->blocks.back()->parent_function = ir_member_function;
            ir_builder->pushBlock(ir_member_function->blocks.back());
            ir_builder->inserter_iterator = ir_builder->currentBlock()->values.end();

            auto ir_this_pointer = ir_member_function->parameters.front();
            // 这里叫函数指针, 函数的类型就是函数指针
            auto ir_function_pointer = ir_builder->create<ir::AccessField>(
                ir_builder->create<ir::DeferencePointer>(ir_this_pointer), field_function_pointer);
            // 直接将外层函数的参数转发进去, 除了第一个参数需要调整一下
            auto ir_arguments = ir_member_function->parameters;
            ir_arguments.front() = ir_builder->accessField(
                ir_builder->create<ir::AccessField>(
                    ir_builder->create<ir::DeferencePointer>(ir_this_pointer),
                    field_object_pointer),
                "raw_ptr");
            auto ir_function_call = ir_builder->create<ir::Call>(ir_function_pointer, ir_arguments);
            ir_builder->create<ir::Return>(ir_function_call);

            ir_interface->functions.push_back(ir_member_function);
        }

        ir_builder->this_pointer_type = nullptr;

        ir_interface_struct->interface_dict["Self"] = ir_interface;
    }

    Symbol operator()(ast::Blank) { return nullptr; }

    template <typename Statement_>
    Symbol operator()(Statement_ ast_statement) {
        PRAJNA_ASSERT(false, typeid(ast_statement).name());
        return nullptr;
    }

    std::shared_ptr<ir::Value> applyExpression(ast::Expression ast_expression) {
        return expression_lowering_visitor->apply(ast_expression);
    }

   public:
    std::shared_ptr<ExpressionLoweringVisitor> expression_lowering_visitor = nullptr;
    std::shared_ptr<Compiler> compiler = nullptr;
    std::shared_ptr<IrBuilder> ir_builder = nullptr;
    std::shared_ptr<Logger> logger = nullptr;
};

}  // namespace prajna::lowering
