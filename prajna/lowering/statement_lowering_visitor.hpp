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

inline bool IsInitializable(std::shared_ptr<ir::Type> ir_type) {
    return ir_type->GetMemberFunction("__initialize__") != nullptr;
}

inline bool HasInitializable(std::shared_ptr<ir::Type> ir_type) {
    if (IsInitializable(ir_type)) {
        return true;
    }

    if (auto ir_struct_type = Cast<ir::StructType>(ir_type)) {
        for (auto ir_field : ir_struct_type->fields) {
            if (HasInitializable(ir_field->type)) {
                return true;
            }
        }
    }

    return false;
}

inline void InitializeVariableLikedCallback(std::shared_ptr<ir::VariableLiked> ir_variable_liked,
                                            std::shared_ptr<lowering::IrBuilder> ir_builder) {
    auto ir_type = ir_variable_liked->type;
    if (HasInitializable(ir_type)) {
        if (auto is_struct_type = Cast<ir::StructType>(ir_type)) {
            for (auto ir_field : is_struct_type->fields) {
                if (HasInitializable(ir_field->type)) {
                    auto ir_access_field =
                        ir_builder->Create<ir::AccessField>(ir_variable_liked, ir_field);
                    InitializeVariableLikedCallback(ir_access_field, ir_builder);
                }
            }
        }

        if (IsInitializable(ir_type)) {
            auto ir_function = ir_type->GetMemberFunction("__initialize__");
            ir_builder->CallMemberFunction(ir_variable_liked, ir_function, {});
        };
    }
}

inline bool IsCopy(std::shared_ptr<ir::Type> ir_type) {
    return ir_type->GetMemberFunction("__copy__") != nullptr;
}

inline bool HasCopy(std::shared_ptr<ir::Type> ir_type) {
    if (IsCopy(ir_type)) {
        return true;
    }

    if (auto ir_struct_type = Cast<ir::StructType>(ir_type)) {
        for (auto ir_field : ir_struct_type->fields) {
            if (HasCopy(ir_field->type)) {
                return true;
            }
        }
    }

    return false;
}

inline bool IsFinalize(std::shared_ptr<ir::Type> ir_type) {
    return ir_type->GetMemberFunction("__finalize__") != nullptr;
}

inline bool HasFinalize(std::shared_ptr<ir::Type> ir_type) {
    if (IsFinalize(ir_type)) {
        return true;
    }

    if (auto ir_struct_type = Cast<ir::StructType>(ir_type)) {
        for (auto ir_field : ir_struct_type->fields) {
            if (HasFinalize(ir_field->type)) {
                return true;
            }
        }
    }

    return false;
}

inline void FinalizeVariableLikedCallback(std::shared_ptr<ir::Value> ir_value,
                                          std::shared_ptr<lowering::IrBuilder> ir_builder) {
    auto ir_type = ir_value->type;
    if (HasFinalize(ir_type)) {
        auto ir_variable_liked = ir_builder->VariableLikedNormalize(ir_value);
        // 和incresement的顺序是相反的,
        if (IsFinalize(ir_type)) {
            auto ir_function = ir_type->GetMemberFunction("__finalize__");
            ir_builder->CallMemberFunction(ir_variable_liked, ir_function, {});
        };

        if (auto is_struct_type = Cast<ir::StructType>(ir_type)) {
            // 按声明相反的顺序处理
            for (auto iter_field = is_struct_type->fields.rbegin();
                 iter_field != is_struct_type->fields.rend(); ++iter_field) {
                auto ir_field = *iter_field;
                if (HasFinalize(ir_field->type)) {
                    auto ir_access_field =
                        ir_builder->Create<ir::AccessField>(ir_variable_liked, ir_field);
                    FinalizeVariableLikedCallback(ir_access_field, ir_builder);
                }
            }
        }
    }
}

/// @brief
/// @param ir_value
/// @param ir_builder
/// @return
inline void CopyVariableLikedCallback(std::shared_ptr<ir::Value> ir_value,
                                      std::shared_ptr<lowering::IrBuilder> ir_builder) {
    auto ir_type = ir_value->type;
    if (HasCopy(ir_type)) {
        auto ir_variable_liked = ir_builder->VariableLikedNormalize(ir_value);
        if (auto ir_struct_type = Cast<ir::StructType>(ir_type)) {
            for (auto ir_field : ir_struct_type->fields) {
                if (HasCopy(ir_field->type)) {
                    auto ir_access_field =
                        ir_builder->Create<ir::AccessField>(ir_variable_liked, ir_field);
                    CopyVariableLikedCallback(ir_access_field, ir_builder);
                }
            }
        }

        if (IsCopy(ir_type)) {
            auto ir_function = ir_type->GetMemberFunction("__copy__");
            ir_builder->CallMemberFunction(ir_variable_liked, ir_function, {});
        }
    }
}

class StatementLoweringVisitor : public std::enable_shared_from_this<StatementLoweringVisitor> {
    StatementLoweringVisitor() = default;

    using result_type = Symbol;

   public:
    static std::shared_ptr<StatementLoweringVisitor> Create(
        std::shared_ptr<lowering::SymbolTable> symbol_table, std::shared_ptr<Logger> logger,
        std::shared_ptr<ir::Module> ir_module, std::shared_ptr<Compiler> compiler) {
        std::shared_ptr<StatementLoweringVisitor> self(new StatementLoweringVisitor);
        PRAJNA_ASSERT(ir_module);
        // if (!ir_module) ir_module = ir::Module::Create();
        self->ir_builder = IrBuilder::Create(symbol_table, ir_module, logger);
        self->logger = logger;
        self->compiler = compiler;
        self->expression_lowering_visitor =
            ExpressionLoweringVisitor::Create(self->ir_builder, logger);
        self->expression_lowering_visitor->wp_statement_lowering_visitor = self->shared_from_this();
        return self;
    }

    std::shared_ptr<ir::Module> Apply(std::shared_ptr<ast::Statements> sp_ast_statements) {
        (*this)(*sp_ast_statements);
        ir_builder->module->symbol_table = ir_builder->symbol_table;
        return ir_builder->module;
    }

    Symbol operator()(ast::Module ast_module) {
        auto symbol_table = ir_builder->symbol_table;
        for (auto ast_template_identifier : ast_module.name.identifiers) {
            if (ast_template_identifier.template_arguments_optional) {
                logger->Error("the invalid module name",
                              ast_template_identifier.template_arguments_optional.get());
            }
            auto ast_module_name = ast_template_identifier.identifier;
            if (!symbol_table->CurrentTableHas(ast_module_name)) {
                symbol_table->Set(SymbolTable::Create(symbol_table), ast_module_name);
            }
            symbol_table = SymbolGet<SymbolTable>(symbol_table->Get(ast_module_name));
            if (!symbol_table) {
                logger->Error("is not a module", ast_module_name);
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
            logger->Note(ast_module.name);
            throw compile_error;
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
        ir_builder->PushSymbolTable();
        ir_builder->CreateAndPushBlock();
        auto guard = ScopeExit::Create([this]() {
            this->ir_builder->PopSymbolTable();
            this->ir_builder->PopBlock();
        });

        (*this)(block.statements);
        auto ir_block = ir_builder->CurrentBlock();
        return ir_block;
    }

    Symbol operator()(ast::VariableDeclaration ast_variable_declaration,
                      bool use_global_variable = false) {
        std::shared_ptr<ir::Type> ir_type = nullptr;
        std::shared_ptr<ir::Value> ir_initial_value = nullptr;
        if (ast_variable_declaration.type_optional) {
            ir_type = this->ApplyType(ast_variable_declaration.type_optional.get());
        }
        // 确定变量的type
        if (ast_variable_declaration.initialize_optional) {
            ir_initial_value = expression_lowering_visitor->Apply(
                ast_variable_declaration.initialize_optional.get());

            if (!ir_type) {
                ir_type = ir_initial_value->type;
            } else {
                if (ir_type != ir_initial_value->type) {
                    logger->Error(fmt::format("the declaration type is \"{}\", but the initialize "
                                              "value's type is \"{}\"",
                                              ir_type->name, ir_initial_value->type->name),
                                  ast_variable_declaration.initialize_optional.get());
                }
            }
        }

        // 类型不确定
        if (!ir_type) {
            logger->Error("the declaration type is unsure", ast_variable_declaration);
        }

        std::shared_ptr<ir::VariableLiked> ir_variable_liked;
        if (use_global_variable) {
            auto ir_global_variable = ir::GlobalVariable::Create(ir_type);
            ir_builder->module->AddGlobalVariable(ir_global_variable);
            ir_variable_liked = ir_global_variable;
        } else {
            ir_variable_liked = ir_builder->Create<ir::LocalVariable>(ir_type);
        }
        ir_variable_liked->annotation_dict =
            this->ApplyAnnotations(ast_variable_declaration.annotation_dict);
        ir_builder->SetSymbol(ir_variable_liked, ast_variable_declaration.name);

        if (ir_initial_value) {
            auto write_variable =
                ir_builder->Create<ir::WriteVariableLiked>(ir_initial_value, ir_variable_liked);
        }

        return ir_variable_liked;
    }

    Symbol operator()(ast::Assignment ast_assignment) {
        // 右值应该先解析
        auto ir_rhs = expression_lowering_visitor->applyOperand(ast_assignment.right);
        auto ir_lhs = expression_lowering_visitor->applyOperand(ast_assignment.left);

        if (auto ir_variable_liked = Cast<ir::VariableLiked>(ir_lhs)) {
            if (ir_variable_liked->type != ir_rhs->type) {
                logger->Error("the type is not matched", ast_assignment);
            }
            ir_builder->Create<ir::WriteVariableLiked>(ir_rhs, ir_variable_liked);
            return ir_lhs;
        }

        if (auto ir_property = Cast<ir::AccessProperty>(ir_lhs)) {
            if (!ir_property->property->set_function) {
                logger->Error("the property has not a setter function", ast_assignment.left);
            }
            if (ir_property->property->set_function->function_type->parameter_types.back() !=
                ir_rhs->type) {
                logger->Error("the type is not matched", ast_assignment);
            }
            ir_builder->Create<ir::WriteProperty>(ir_rhs, ir_property);
            return ir_lhs;
        }

        logger->Error("the left side is not writeable", ast_assignment.left);

        return nullptr;
    }

    std::list<std::shared_ptr<ir::Type>> ApplyParameters(std::list<ast::Parameter> ast_parameters) {
        std::list<std::shared_ptr<ir::Type>> types(ast_parameters.size());
        std::transform(RANGE(ast_parameters), types.begin(), [=](ast::Parameter ast_parameter) {
            return this->ApplyType(ast_parameter.type);
        });

        return types;
    }

    std::unordered_map<std::string, std::list<std::string>> ApplyAnnotations(
        ast::AnnotationDict ast_annotations) {
        std::unordered_map<std::string, std::list<std::string>> annotation_dict;
        for (auto ast_annotation : ast_annotations) {
            std::list<std::string> values;
            std::transform(RANGE(ast_annotation.values), std::back_inserter(values),
                           [](ast::StringLiteral string_literal) { return string_literal.value; });
            annotation_dict.insert({ast_annotation.name, values});
        }
        return annotation_dict;
    }

    bool AllBranchIsTerminated(std::shared_ptr<ir::Block> ir_block,
                               std::shared_ptr<ir::Type> ir_type) {
        PRAJNA_ASSERT(!Is<ir::VoidType>(ir_type));
        auto ir_last_value = ir_block->back();
        if (auto ir_return = Cast<ir::Return>(ir_last_value)) {
            return ir_return->Value() && ir_return->Value()->type == ir_type;
        }
        if (auto ir_if = Cast<ir::If>(ir_last_value)) {
            auto re = this->AllBranchIsTerminated(ir_if->TrueBlock(), ir_type) &&
                      this->AllBranchIsTerminated(ir_if->FalseBlock(), ir_type);
            if (re) {
                // //插入一个Return协助判断
                ir_builder->PushBlock(ir_if->GetParentBlock());
                auto ir_variable = ir_builder->Create<ir::LocalVariable>(ir_type);
                auto ir_return = ir_builder->Create<ir::Return>(ir_variable);
                ir_builder->PopBlock();

                return true;
            } else {
                return false;
            }
        }
        if (auto ir_block_ = Cast<ir::Block>(ir_last_value)) {
            return this->AllBranchIsTerminated(ir_block_, ir_type);
        }

        return false;
    }

    std::shared_ptr<ir::Function> ApplyFunctionHeader(ast::FunctionHeader ast_function_header) {
        std::shared_ptr<ir::Type> return_type;
        if (ast_function_header.return_type_optional) {
            return_type = ApplyType(ast_function_header.return_type_optional.get());
        } else {
            return_type = ir::VoidType::Create();
        }
        auto ir_argument_types = ApplyParameters(ast_function_header.parameters);
        // 插入this-pointer type
        if (ir_builder->IsBuildingMemberfunction()) {
            ir_argument_types.insert(ir_argument_types.begin(),
                                     ir::PointerType::Create(ir_builder->current_implement_type));
        }

        auto ir_function_type = ir::FunctionType::Create(ir_argument_types, return_type);

        auto ir_function = ir_builder->CreateFunction(ast_function_header.name, ir_function_type);
        ir_function->annotation_dict = this->ApplyAnnotations(ast_function_header.annotation_dict);

        // 加入interface里
        if (ir_builder->current_implement_interface) {
            ir_builder->current_implement_interface->functions.push_back(ir_function);
        } else {
            // 加入struct里
            if (ir_builder->current_implement_type) {
                if (ir_builder->is_static_function) {
                    ir_builder->current_implement_type->static_function_dict[ir_function->name] =
                        ir_function;
                } else {
                    ir_builder->current_implement_type->member_function_dict[ir_function->name] =
                        ir_function;
                }
            }
        }

        return ir_function;
    }

    Symbol operator()(ast::Function ast_function) {
        try {
            ir_builder->is_static_function =
                std::any_of(RANGE(ast_function.declaration.annotation_dict),
                            [](auto x) { return x.name == "static"; });

            auto ir_function = ApplyFunctionHeader(ast_function.declaration);
            ir_function->source_location = ast_function.declaration.name;
            // 进入参数域,

            // @note 将function的第一个block作为最上层的block
            if (ast_function.body_optional) {
                ir_builder->PushSymbolTable();
                ir_builder->CreateTopBlockForFunction(ir_function);
                auto guard = ScopeExit::Create([=]() {
                    this->ir_builder->PopBlock();
                    this->ir_builder->function_stack.pop();
                    this->ir_builder->PopSymbolTable();
                });

                auto iter_argument = ir_function->parameters.begin();
                if (ir_builder->IsBuildingMemberfunction()) {
                    // Argument也不会插入的block里
                    ir_builder->symbol_table->SetWithAssigningName(ir_function->parameters.front(),
                                                                   "this-pointer");
                    ++iter_argument;
                }

                for (auto iter_parameter = ast_function.declaration.parameters.begin();
                     iter_parameter != ast_function.declaration.parameters.end();
                     ++iter_argument, ++iter_parameter) {
                    ir_builder->SetSymbol(*iter_argument, iter_parameter->name);
                }

                if (ir_builder->IsBuildingMemberfunction()) {
                    auto ir_this_pointer =
                        SymbolGet<ir::Value>(ir_builder->symbol_table->Get("this-pointer"));
                    auto ir_this = ir_builder->Create<ir::DeferencePointer>(ir_this_pointer);
                    ir_builder->symbol_table->SetWithAssigningName(ir_this, "this");
                }

                (*this)(ast_function.body_optional.get());

                // void返回类型直接补一个Return即可, 让后端去优化冗余的指令
                if (Is<ir::VoidType>(ir_function->function_type->return_type)) {
                    ir_builder->Create<ir::Return>(ir_builder->Create<ir::VoidValue>());
                } else {
                    if (!this->AllBranchIsTerminated(ir_function->blocks.back(),
                                                     ir_function->function_type->return_type)) {
                        logger->Error("not all branch have been terminated",
                                      ast_function.declaration);
                    }
                }
            } else {
                ir_function->blocks.clear();
            }

            ir_builder->is_static_function = false;

            return ir_function;
        } catch (CompileError compile_error) {
            logger->Note(ast_function.declaration);
            throw compile_error;
        }
    }

    Symbol operator()(ast::Return ast_return) {
        std::shared_ptr<ir::Return> ir_return;
        if (ast_return.expr_optional) {
            auto ir_return_value =
                expression_lowering_visitor->Apply(ast_return.expr_optional.get());
            ir_return = ir_builder->Create<ir::Return>(ir_return_value);
        } else {
            auto ir_void_value = ir_builder->Create<ir::VoidValue>();
            ir_return = ir_builder->Create<ir::Return>(ir_void_value);
        }

        auto ir_return_type = ir_builder->function_stack.top()->function_type->return_type;
        if (ir_return->type != ir_return_type) {
            logger->Error(fmt::format("the type is {} , but then function return type is {}",
                                      ir_return->type->fullname, ir_return_type->fullname),
                          ast_return);
        }

        return ir_return;
    }

    Symbol operator()(ast::If ast_if) {
        auto ir_condition = ApplyExpression(ast_if.condition);
        auto ir_if =
            ir_builder->Create<ir::If>(ir_condition, ir::Block::Create(), ir::Block::Create());
        ir_if->TrueBlock()->parent = ir_if;
        ir_if->FalseBlock()->parent = ir_if;

        ir_builder->PushBlock(ir_if->TrueBlock());
        (*this)(ast_if.then);
        ir_builder->PopBlock();

        ir_builder->PushBlock(ir_if->FalseBlock());
        if (ast_if.else_optional) {
            (*this)(ast_if.else_optional.get());
        }
        ir_builder->PopBlock();

        return ir_if;
    }

    Symbol operator()(ast::While ast_while) {
        auto ir_condition_block = ir::Block::Create();
        ir_builder->PushBlock(ir_condition_block);
        auto ir_condition = ApplyExpression(ast_while.condition);
        ir_builder->PopBlock();

        auto ir_loop_block = ir::Block::Create();
        auto ir_while =
            ir_builder->Create<ir::While>(ir_condition, ir_condition_block, ir_loop_block);
        ir_builder->loop_stack.push(ir_while);
        ir_builder->PushBlock(ir_while->LoopBlock());
        (*this)(ast_while.body);
        ir_builder->PopBlock();
        ir_builder->loop_stack.pop();

        return ir_while;
    }

    Symbol operator()(ast::For ast_for) {
        auto ir_first = expression_lowering_visitor->Apply(ast_for.first);
        auto ir_last = expression_lowering_visitor->Apply(ast_for.last);
        if (ir_last->type != ir_builder->GetInt64Type() &&
            !ir_builder->IsArrayI64Type(ir_last->type)) {
            logger->Error("the index type must be i64 or i64 array", ast_for.last);
        }
        if (ir_last->type != ir_first->type) {
            logger->Error("the last firt type are not matched", ast_for.first);
        }

        // 这里使用了局部值拷贝的方式来模仿右值, 因为ir_first/last_value不会被改变
        auto ir_first_value = ir_builder->CloneValue(ir_first);
        auto ir_last_value = ir_builder->CloneValue(ir_last);
        auto ir_loop_block = ir::Block::Create();
        auto ir_index = ir_builder->Create<ir::LocalVariable>(ir_last->type);
        auto ir_for =
            ir_builder->Create<ir::For>(ir_index, ir_first_value, ir_last_value, ir_loop_block);
        // 迭代变量应该在下一层迭代空间
        {
            ir_builder->loop_stack.push(ir_for);
            ir_builder->PushSymbolTable();
            ir_builder->SetSymbol(ir_index, ast_for.index);
            ir_builder->PushBlock(ir_for->LoopBlock());
            auto guard = ScopeExit::Create([this]() {
                this->ir_builder->PopBlock();
                this->ir_builder->PopSymbolTable();
                this->ir_builder->loop_stack.pop();
            });
            (*this)(ast_for.body);
        }

        for (auto ast_annotation : ast_for.annotation_dict) {
            std::list<std::string> values;
            std::transform(RANGE(ast_annotation.values), std::back_inserter(values),
                           [](ast::StringLiteral string_literal) { return string_literal.value; });
            ir_for->annotation_dict.insert({ast_annotation.name, values});
        }

        ir_for->annotation_dict = ApplyAnnotations(ast_for.annotation_dict);

        return ir_for;
    }

    Symbol operator()(ast::Break ast_break) {
        if (!ir_builder->loop_stack.size()) {
            logger->Error("break is outside of a loop", ast_break);
        }
        return ir_builder->Create<ir::Break>(ir_builder->loop_stack.top());
    }

    Symbol operator()(ast::Continue ast_continue) {
        if (!ir_builder->loop_stack.size()) {
            logger->Error("continue is outside of a loop", ast_continue);
        }
        return ir_builder->Create<ir::Continue>(ir_builder->loop_stack.top());
    }

    std::shared_ptr<ir::Type> ApplyType(ast::Type ast_postfix_type) {
        return expression_lowering_visitor->ApplyType(ast_postfix_type);
    }

    Symbol operator()(ast::Struct ast_struct) {
        auto ir_struct_type = ir::StructType::Create();
        ir_builder->instantiating_type_stack.push(ir_struct_type);

        ir_builder->SetSymbolWithTemplateArgumentsPostify(ir_struct_type, ast_struct.name);

        std::list<std::shared_ptr<ir::Field>> ir_fields;
        std::transform(
            RANGE(ast_struct.fields), std::back_inserter(ir_fields), [=](ast::Field ast_field) {
                return ir::Field::Create(ast_field.name, this->ApplyType(ast_field.type));
            });
        ir_struct_type->fields = ir_fields;
        ir_struct_type->Update();

        ir_builder->instantiating_type_stack.pop();

        return ir_struct_type;
    }

    Symbol operator()(ast::ImplementType ast_implement) {
        try {
            auto ir_type = expression_lowering_visitor->ApplyType(ast_implement.type);

            ir_builder->PushSymbolTable();
            ir_builder->symbol_table->name = ir_type->name;
            PRAJNA_ASSERT(!ir_builder->current_implement_type);
            ir_builder->current_implement_type = ir_type;
            auto guard = ScopeExit::Create([this]() {
                this->ir_builder->PopSymbolTable();
                this->ir_builder->current_implement_type = nullptr;
            });

            for (auto ast_statement : ast_implement.statements) {
                auto symbol_function = (*this)(ast_statement);
                if (auto ir_function = Cast<ir::Function>(SymbolGet<ir::Value>(symbol_function))) {
                    continue;
                }
                if (auto lowering_template = SymbolGet<Template>(symbol_function)) {
                    ir_type->template_any_dict[lowering_template->name] = lowering_template;
                    continue;
                }
                PRAJNA_TODO;
            }
        } catch (CompileError compile_error) {
            logger->Note(ast_implement.type);
            throw compile_error;
        }

        return nullptr;
    }

    Symbol operator()(ast::ImplementInterfaceForType ast_implement) {
        try {
            auto ir_type = expression_lowering_visitor->ApplyType(ast_implement.type);

            ir_builder->PushSymbolTable();
            ir_builder->symbol_table->name = ir_type->name;
            PRAJNA_ASSERT(!ir_builder->current_implement_type);
            ir_builder->current_implement_type = ir_type;

            auto ir_interface = ir::InterfaceImplement::Create();
            auto ir_interface_prototype = SymbolGet<ir::InterfacePrototype>(
                expression_lowering_visitor->ApplyIdentifierPath(ast_implement.interface));
            if (!ir_interface_prototype) {
                logger->Error("not invalid interface", ast_implement.interface);
            }
            ir_interface->name = ir_interface_prototype->name;
            ir_interface->prototype = ir_interface_prototype;
            ir_interface->fullname = ConcatFullname(ir_type->fullname, ir_interface->name);
            if (ir_type->interface_dict[ir_interface->name]) {
                logger->Error("interface has implemented", ast_implement.interface);
            }
            ir_type->interface_dict[ir_interface->name] = ir_interface;

            ir_builder->PushSymbolTable();
            ir_builder->symbol_table->name = ir_interface->name;
            ir_builder->current_implement_interface = ir_interface;

            auto guard = ScopeExit::Create([=]() {
                this->ir_builder->PopSymbolTable();
                this->ir_builder->PopSymbolTable();
                this->ir_builder->current_implement_type = nullptr;
                this->ir_builder->current_implement_interface = nullptr;
            });

            // 是否每个interface prototpye的函数都被实现
            auto is_matched_with_function_prototype =
                [logger = this->logger](std::shared_ptr<ir::Function> ir_function0,
                                        std::shared_ptr<ir::Function> ir_function1) {
                    if (ir_function0->name == ir_function1->name) {
                        if (ir_function0->type == ir_function1->type) {
                            return true;
                        } else {
                            logger->Error("function type is not matched",
                                          ir_function0->source_location);
                        }
                    }
                    return false;
                };

            // 创建接口动态类型生成函数
            ir_interface->dynamic_type_creator = ir_builder->CreateFunction(
                std::string("dynamic_type_creator"),
                ir::FunctionType::Create({ir_builder->GetManagedPtrType(ir_type)},
                                         ir_interface->prototype->dynamic_type));

            auto is_matched_with_prototype =
                [](std::shared_ptr<ir::FunctionType> ir_function_type0,
                   std::shared_ptr<ir::FunctionType> ir_function_type1) {
                    if (ir_function_type0->return_type != ir_function_type1->return_type) {
                        return false;
                    }
                    // 不比较第一个参数
                    auto iter0 = std::next(ir_function_type0->parameter_types.begin());
                    auto iter1 = ir_function_type1->parameter_types.begin();
                    for (; iter0 != ir_function_type0->parameter_types.end(); ++iter0, ++iter1) {
                        if (*iter0 != *iter1) {
                            return false;
                        }
                    }

                    return true;
                };

            auto ir_function_prototype_list = ir_interface_prototype->functions;
            for (auto ast_function : ast_implement.functions) {
                auto symbol_function = (*this)(ast_function);
                auto ir_function = Cast<ir::Function>(SymbolGet<ir::Value>(symbol_function));
                auto iter = std::find_if(
                    RANGE(ir_function_prototype_list),
                    [=](std::shared_ptr<ir::Function> ir_function_prototype) {
                        if (ir_function->name == ir_function_prototype->name) {
                            if (is_matched_with_prototype(ir_function->function_type,
                                                          ir_function_prototype->function_type)) {
                                return true;
                            } else {
                                logger->Error("function type is not matched",
                                              ast_function.declaration);
                                return false;
                            }
                        } else {
                            return false;
                        }
                    });
                if (iter != ir_function_prototype_list.end()) {
                    ir_function_prototype_list.erase(iter);
                } else {
                    this->logger->Error("has no interface function prototype",
                                        ast_function.declaration);
                }
            }

            for (auto ir_function_prototype : ir_function_prototype_list) {
                // 只会打印第一个错误, 因为会抛出异常
                this->logger->Error("the interface function is not implement",
                                    ir_function_prototype->source_location);
            }

            // 包装一个undef this pointer的函数
            for (auto ir_function : ir_interface->functions) {
                if (std::none_of(RANGE(ir_interface_prototype->functions),
                                 [=](auto x) { return x->name == ir_function->name; })) {
                    continue;
                }

                std::list<std::shared_ptr<ir::Type>> ir_undef_this_pointer_function_argument_types =
                    ir_function->function_type->parameter_types;
                ir_undef_this_pointer_function_argument_types.front() =
                    ir::PointerType::Create(ir::UndefType::Create());

                auto ir_undef_this_pointer_function_type =
                    ir::FunctionType::Create(ir_undef_this_pointer_function_argument_types,
                                             ir_function->function_type->return_type);
                auto ir_undef_this_pointer_function = ir_builder->CreateFunction(
                    ir_function->name + "/undef", ir_undef_this_pointer_function_type);

                ir_interface->undef_this_pointer_functions.insert(
                    {ir_function, ir_undef_this_pointer_function});

                ir_builder->CreateTopBlockForFunction(ir_undef_this_pointer_function);
                // 将undef *的this pointer转为本身的指针类型
                auto ir_this_pointer = ir_builder->Create<ir::BitCast>(
                    ir_undef_this_pointer_function->parameters.front(),
                    ir_function->parameters.front()->type);
                std::list<std::shared_ptr<ir::Value>> ir_arguments;
                std::transform(RANGE(ir_undef_this_pointer_function->parameters),
                               std::back_inserter(ir_arguments),
                               [](auto x) -> std::shared_ptr<ir::Value> { return x; });
                ir_arguments.front() = ir_this_pointer;
                auto ir_call = ir_builder->Call(ir_function, ir_arguments);
                if (Is<ir::VoidType>(ir_call->type)) {
                    ir_builder->Create<ir::Return>(ir_builder->Create<ir::VoidValue>());
                } else {
                    ir_builder->Create<ir::Return>(ir_call);
                }
                ir_builder->PopBlock();
            }

            ir_builder->CreateTopBlockForFunction(ir_interface->dynamic_type_creator);

            auto ir_self =
                ir_builder->Create<ir::LocalVariable>(ir_interface->prototype->dynamic_type);

            ir_builder->Create<ir::WriteVariableLiked>(
                ir_builder->CallMemberFunction(
                    ir_interface->dynamic_type_creator->parameters.front(), "ToUndef", {}),
                ir_builder->AccessField(ir_self, "object_pointer"));
            for (auto ir_function : ir_interface->functions) {
                if (std::none_of(RANGE(ir_interface_prototype->functions),
                                 [=](auto x) { return x->name == ir_function->name; })) {
                    continue;
                }

                auto iter_field = std::find_if(
                    RANGE(ir_interface->prototype->dynamic_type->fields),
                    [=](auto ir_field) { return ir_field->name == ir_function->name + "/fp"; });
                PRAJNA_ASSERT(iter_field != ir_interface->prototype->dynamic_type->fields.end());
                auto ir_function_pointer =
                    ir_builder->Create<ir::AccessField>(ir_self, *iter_field);
                ir_builder->Create<ir::WriteVariableLiked>(
                    ir_interface->undef_this_pointer_functions[ir_function], ir_function_pointer);
            }
            ir_builder->Create<ir::Return>(ir_self);
            ir_builder->PopBlock();
        } catch (CompileError compile_error) {
            logger->Note(ast_implement.type);
            throw compile_error;
        }

        return nullptr;
    }

    std::shared_ptr<Template> GetOrCreateTemplate(ast::Identifier ast_identifier) {
        if (ir_builder->symbol_table->CurrentTableHas(ast_identifier)) {
            auto template_ = SymbolGet<Template>(ir_builder->symbol_table->Get(ast_identifier));
            PRAJNA_ASSERT(template_);
            return template_;
        } else {
            auto template_ = Template::Create();
            ir_builder->SetSymbol(template_, ast_identifier);
            return template_;
        }
    }

    Symbol operator()(ast::Template ast_template) {
        auto template_ = this->GetOrCreateTemplate(ast_template.name);
        template_->generator = this->CreateTemplateGenerator(ast_template.template_parameters,
                                                             ast_template.statements, false);
        return template_;
    }

    std::list<std::string> GetTemplateParametersIdentifiers(
        std::list<ast::TemplateParameter> template_parameters) {
        std::set<ast::Identifier> template_parameter_identifier_set;
        std::list<std::string> template_parameter_identifier_list;
        for (auto template_parameter : template_parameters) {
            if (template_parameter_identifier_set.count(template_parameter.name)) {
                logger->Error("the template parameter is defined already", template_parameter);
            }
            template_parameter_identifier_list.push_back(template_parameter.name);
            template_parameter_identifier_set.insert(template_parameter.name);
        }

        return template_parameter_identifier_list;
    }

    Template::Generator CreateTemplateGenerator(ast::TemplateParameters ast_template_paramters,
                                                ast::Statement ast_statement, bool has_identifier) {
        auto template_parameter_identifier_list =
            this->GetTemplateParametersIdentifiers(ast_template_paramters);
        return [=, symbol_table = ir_builder->symbol_table, logger = this->logger,
                this_pointer_type = ir_builder->current_implement_type](
                   std::list<Symbol> symbol_template_arguments,
                   std::shared_ptr<ir::Module> ir_module) -> Symbol {
            // 包裹一层名字空间, 避免被污染
            auto templates_symbol_table = SymbolTable::Create(symbol_table);
            templates_symbol_table->name = GetTemplateArgumentsPostify(symbol_template_arguments);

            if (template_parameter_identifier_list.size() != symbol_template_arguments.size()) {
                logger->Error("template arguments size si not matched");
            }

            for (auto [identifier, symbol] :
                 boost::combine(template_parameter_identifier_list, symbol_template_arguments)) {
                templates_symbol_table->Set(symbol, identifier);
            }

            auto statement_lowering_visitor = StatementLoweringVisitor::Create(
                templates_symbol_table, logger, ir_module, nullptr);
            try {
                statement_lowering_visitor->ir_builder->current_implement_type = this_pointer_type;
                if (has_identifier) {
                    statement_lowering_visitor->ir_builder->symbol_template_argument_list_optional =
                        symbol_template_arguments;
                }

                auto symbol = (*statement_lowering_visitor)(ast_statement);

                statement_lowering_visitor->ir_builder->current_implement_type = nullptr;
                statement_lowering_visitor->ir_builder->symbol_template_argument_list_optional
                    .reset();
                return symbol;
            } catch (CompileError compile_error) {
                logger->Note(ast_template_paramters);
                throw compile_error;
                return nullptr;
            }
        };
    }

    Symbol operator()(ast::TemplateStatement ast_template_statement) {
        return boost::apply_visitor(
            overloaded{
                [=](ast::Struct ast_struct) -> Symbol {
                    std::shared_ptr<TemplateStruct> template_struct;
                    template_struct = TemplateStruct::Create();
                    ir_builder->SetSymbol(template_struct, ast_struct.name);

                    template_struct->template_struct_impl->generator =
                        this->CreateTemplateGenerator(ast_template_statement.template_parameters,
                                                      ast_struct, true);

                    return template_struct;
                },
                [=](ast::ImplementInterfaceForType ast_implement_type_for_interface) -> Symbol {
                    auto template_parameter_identifier_list = GetTemplateParametersIdentifiers(
                        ast_template_statement.template_parameters);

                    auto ast_template_struct = ast_implement_type_for_interface.type;
                    ast_template_struct.identifiers.back().template_arguments_optional =
                        boost::none;
                    auto ir_symbol =
                        expression_lowering_visitor->ApplyIdentifierPath(ast_template_struct);
                    auto template_struct = SymbolGet<TemplateStruct>(ir_symbol);
                    if (template_struct == nullptr) {
                        logger->Error("it's not a template struct but with template parameters",
                                      ast_implement_type_for_interface.type);
                    }

                    auto template_ = Template::Create();
                    template_->generator =
                        this->CreateTemplateGenerator(ast_template_statement.template_parameters,
                                                      ast_implement_type_for_interface, false);
                    template_struct->template_implement_type_vec.push_back(template_);

                    return template_;
                },
                [=](ast::ImplementType ast_implement_type) -> Symbol {
                    auto template_parameter_identifier_list = GetTemplateParametersIdentifiers(
                        ast_template_statement.template_parameters);

                    auto ast_template_struct = ast_implement_type.type;
                    ast_template_struct.identifiers.back().template_arguments_optional =
                        boost::none;
                    auto ir_symbol =
                        expression_lowering_visitor->ApplyIdentifierPath(ast_template_struct);
                    auto template_struct = SymbolGet<TemplateStruct>(ir_symbol);
                    if (template_struct == nullptr) {
                        logger->Error("it's not a template struct but with template parameters",
                                      ast_implement_type.type);
                    }

                    auto template_ = Template::Create();
                    template_->generator = this->CreateTemplateGenerator(
                        ast_template_statement.template_parameters, ast_implement_type, false);
                    template_struct->template_implement_type_vec.push_back(template_);
                    return template_;
                },
                [=](ast::InterfacePrototype ast_interface_prototype) -> Symbol {
                    auto template_ = this->GetOrCreateTemplate(ast_interface_prototype.name);
                    template_->generator = this->CreateTemplateGenerator(
                        ast_template_statement.template_parameters, ast_interface_prototype, true);
                    return template_;
                },
                [=](ast::Function ast_function) -> Symbol {
                    auto ast_template_paramters = ast_template_statement.template_parameters;
                    auto template_ = this->GetOrCreateTemplate(ast_function.declaration.name);
                    template_->generator = this->CreateTemplateGenerator(
                        ast_template_statement.template_parameters, ast_function, true);
                    return template_;
                }},
            ast_template_statement.statement);
    }

    Symbol operator()(ast::SpecialStatement ast_special_statement) {
        return boost::apply_visitor(
            overloaded{
                [=](auto x) -> Symbol {
                    PRAJNA_UNREACHABLE;
                    return nullptr;
                },
                [=](ast::Struct ast_struct) -> Symbol {
                    auto ir_symbol = ir_builder->symbol_table->Get(ast_struct.name);
                    auto template_struct = SymbolGet<TemplateStruct>(ir_symbol);
                    if (template_struct == nullptr) {
                        logger->Error("it's not a template struct but with template parameters ",
                                      ast_struct.name);
                    }

                    auto symbol_template_arguments =
                        expression_lowering_visitor->ApplyTemplateArguments(
                            ast_special_statement.template_arguments);

                    // 外部使用算子, 必须值捕获
                    auto template_struct_generator = [=](std::shared_ptr<ir::Module> ir_module)
                        -> std::shared_ptr<ir::StructType> {
                        // // 包裹一层名字空间, 避免被污染
                        // TODO 应该有个名字
                        auto templates_symbol_table = SymbolTable::Create(ir_builder->symbol_table);

                        auto statement_lowering_visitor = StatementLoweringVisitor::Create(
                            templates_symbol_table, logger, ir_module, nullptr);
                        auto ir_type = Cast<ir::StructType>(
                            SymbolGet<ir::Type>((*statement_lowering_visitor)(ast_struct)));
                        return ir_type;
                    };

                    PRAJNA_ASSERT(template_struct->template_struct_impl);
                    template_struct->template_struct_impl
                        ->special_generator_dict[symbol_template_arguments] =
                        template_struct_generator;

                    return nullptr;
                }},
            ast_special_statement.statement.get());
    }

    Symbol operator()(ast::Expression ast_expression) {
        return this->ApplyExpression(ast_expression);
    }

    Symbol operator()(ast::Use ast_import);

    std::shared_ptr<Template> CreateCastInstructionTemplate() {
        auto template_cast_instruction = Template::Create();
        template_cast_instruction->generator =
            [symbol_table = this->ir_builder->symbol_table, logger = this->logger, this](
                std::list<Symbol> symbol_template_arguments,
                std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 2) {
                logger->Error("should give 2 template arguments");
            }
            auto ir_source_type = SymbolGet<ir::Type>(symbol_template_arguments.front());
            auto ir_target_type = SymbolGet<ir::Type>(symbol_template_arguments.back());
            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_function_type = ir::FunctionType::Create({ir_source_type}, ir_target_type);
            auto ir_function = ir_tmp_builder->CreateFunction(
                "cast" + GetTemplateArgumentsPostify(symbol_template_arguments), ir_function_type);
            ir_function->annotation_dict["inline"];
            ir_tmp_builder->CreateTopBlockForFunction(ir_function);

            if (ir_source_type == ir_target_type) {
                ir_tmp_builder->Create<ir::Return>(ir_function->parameters.front());
                return ir_function;
            }

            auto cast_operation = ir::CastInstruction::Operation::None;

            if (auto ir_source_int_type = Cast<ir::IntType>(ir_source_type)) {
                if (Is<ir::FloatType>(ir_target_type)) {
                    cast_operation = ir_source_int_type->is_signed
                                         ? ir::CastInstruction::Operation::SIToFP
                                         : ir::CastInstruction::Operation::UIToFP;
                }
                if (auto ir_target_int_type = Cast<ir::IntType>(ir_target_type)) {
                    if (ir_source_int_type->bits == ir_target_int_type->bits) {
                        cast_operation = ir::CastInstruction::Operation::BitCast;
                    }
                    if (ir_source_int_type->bits > ir_target_int_type->bits) {
                        cast_operation = ir::CastInstruction::Operation::Trunc;
                    }
                    if (ir_source_int_type->bits < ir_target_int_type->bits) {
                        cast_operation = ir_target_int_type->is_signed
                                             ? ir::CastInstruction::Operation::SExt
                                             : ir::CastInstruction::Operation::ZExt;
                    }
                }
                if (Is<ir::PointerType>(ir_target_type)) {
                    cast_operation = ir::CastInstruction::Operation::IntToPtr;
                }
            }

            if (auto ir_source_float_type = Cast<ir::FloatType>(ir_source_type)) {
                if (auto ir_target_float_type = Cast<ir::FloatType>(ir_target_type)) {
                    cast_operation = ir_source_float_type->bits > ir_target_float_type->bits
                                         ? ir::CastInstruction::Operation::FPTrunc
                                         : ir::CastInstruction::Operation::FPExt;
                }
                if (auto ir_target_int_type = Cast<ir::IntType>(ir_target_type)) {
                    cast_operation = ir_target_int_type->is_signed
                                         ? ir::CastInstruction::Operation::FPToSI
                                         : ir::CastInstruction::Operation::FPToUI;
                }
            }

            if (Is<ir::PointerType>(ir_source_type)) {
                if (Is<ir::IntType>(ir_target_type)) {
                    cast_operation = ir::CastInstruction::Operation::PtrToInt;
                }
            }

            if (cast_operation == ir::CastInstruction::Operation::None) {
                logger->Error("invalid Cast");
            }

            ir_tmp_builder->Create<ir::Return>(ir_tmp_builder->Create<ir::CastInstruction>(
                cast_operation, ir_function->parameters.front(), ir_target_type));
            return ir_function;
        };

        return template_cast_instruction;
    }

    std::shared_ptr<TemplateStruct> CreateIntTypeTemplate(bool is_signed) {
        auto template_int = Template::Create();

        template_int->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                   logger = this->logger](
                                      std::list<Symbol> symbol_template_arguments,
                                      std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 1) {
                logger->Error("should input 1 template argument");
            }
            auto ir_constant_bit_size =
                SymbolGet<ir::ConstantInt>(symbol_template_arguments.front());
            if (!ir_constant_bit_size || ir_constant_bit_size->value <= 0) {
                logger->Error("int bits should be a positive constant int");
            }
            return ir::IntType::Create(ir_constant_bit_size->value, is_signed);
        };

        auto template_struct_int = TemplateStruct::Create();
        template_struct_int->template_struct_impl = template_int;

        return template_struct_int;
    }

    std::shared_ptr<TemplateStruct> CreateFloatTypeTemplate() {
        auto template_int = Template::Create();

        template_int->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                   logger = this->logger](
                                      std::list<Symbol> symbol_template_arguments,
                                      std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 1) {
                logger->Error("should input 1 template argument");
            }
            auto ir_constant_bit_size =
                SymbolGet<ir::ConstantInt>(symbol_template_arguments.front());
            if (!ir_constant_bit_size || ir_constant_bit_size->value <= 0) {
                logger->Error("int bits should be a positive constant int");
            }
            auto bits = ir_constant_bit_size->value;
            // LLVM只支持float16/32/16,  bfloat需要另外的函数去实现, float128支持并不好, sin,
            // cos等函数都是错误的结果
            if (!(bits == 16 || bits == 32 || bits == 64 /*|| bits == 128*/)) {
                logger->Error("float support only 16, 32, 64 bits");
            }
            return ir::FloatType::Create(ir_constant_bit_size->value);
        };

        auto template_struct_float = TemplateStruct::Create();
        template_struct_float->template_struct_impl = template_int;
        return template_struct_float;
    }

    std::shared_ptr<Template> CreateFloatTypeIntrinsicUnaryFunctionTemplate(
        std::string intrinsic_name, int64_t argument_size = 1) {
        auto template_intrinsic = Template::Create();

        template_intrinsic->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                         logger = this->logger](
                                            std::list<Symbol> symbol_template_arguments,
                                            std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 1) {
                logger->Error("should input 1 template argument");
            }

            auto ir_float_type =
                Cast<ir::FloatType>(SymbolGet<ir::Type>(symbol_template_arguments.front()));
            if (!ir_float_type) {
                logger->Error("should be a float type");
            }

            std::list<std::shared_ptr<ir::Type>> ir_argument_list(argument_size, ir_float_type);
            auto ir_function_type = ir::FunctionType::Create(ir_argument_list, ir_float_type);
            // 声明所需intrinsic
            auto ir_intrinsic_function = ir::Function::Create(ir_function_type);
            ir_intrinsic_function->fullname = "llvm." + intrinsic_name + "." + ir_float_type->name;
            ir_module->AddFunction(ir_intrinsic_function);

            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_function = ir_tmp_builder->CreateFunction(
                "__" + intrinsic_name + GetTemplateArgumentsPostify(symbol_template_arguments),
                ir_function_type);
            ir_function->annotation_dict["inline"];
            ir_tmp_builder->CreateTopBlockForFunction(ir_function);

            ir_tmp_builder->Create<ir::Return>(ir_tmp_builder->Call(
                ir_intrinsic_function, ListCast<ir::Value>(ir_function->parameters)));
            return ir_function;
        };

        return template_intrinsic;
    }

    std::shared_ptr<TemplateStruct> CreateRawArrayTypeTemplate() {
        auto template_raw_array = Template::Create();
        template_raw_array->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                         logger = this->logger](
                                            std::list<Symbol> symbol_template_arguments,
                                            std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 2) {
                logger->Error("should input 2 template argument");
            }
            auto ir_type = SymbolGet<ir::Type>(symbol_template_arguments.front());
            if (!ir_type) {
                logger->Error("should be a type");
            }

            auto ir_constant_length = SymbolGet<ir::ConstantInt>(symbol_template_arguments.back());
            if (!ir_constant_length || ir_constant_length->value <= 0) {
                logger->Error("array length should be a positive constant int");
            }

            return ir::ArrayType::Create(ir_type, ir_constant_length->value);
        };

        auto template_struct_raw_array = TemplateStruct::Create();
        template_struct_raw_array->template_struct_impl = template_raw_array;
        return template_struct_raw_array;
    }

    std::shared_ptr<TemplateStruct> CreateRawPtrTypeTemplate() {
        auto template_raw_ptr = Template::Create();
        template_raw_ptr->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                       logger = this->logger](
                                          std::list<Symbol> symbol_template_arguments,
                                          std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 1) {
                logger->Error("should input 1 template argument");
            }
            auto ir_type = SymbolGet<ir::Type>(symbol_template_arguments.front());
            if (!ir_type || Is<ir::VoidType>(ir_type)) {
                logger->Error("should be a first class type");
            }

            return ir::PointerType::Create(ir_type);
        };

        auto template_struct_raw_ptr = TemplateStruct::Create();
        template_struct_raw_ptr->template_struct_impl = template_raw_ptr;
        return template_struct_raw_ptr;
    }

    std::shared_ptr<TemplateStruct> CreateFunctionTypePointerTemplate() {
        auto template_function_type = Template::Create();
        template_function_type->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                             logger = this->logger](
                                                std::list<Symbol> symbol_template_arguments,
                                                std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() < 1) {
                logger->Error("should input at least 1 template argument");
            }

            std::list<std::shared_ptr<ir::Type>> ir_type_list;
            std::transform(RANGE(symbol_template_arguments), std::back_inserter(ir_type_list),
                           [=](auto symbol_template_argument) {
                               auto ir_type = SymbolGet<ir::Type>(symbol_template_argument);
                               if (!ir_type) {
                                   logger->Error("should be types");
                               }
                               return ir_type;
                           });

            auto ir_parameter_list = ir_type_list;
            ir_parameter_list.pop_back();
            auto ir_return_type = ir_type_list.back();

            return ir::PointerType::Create(
                ir::FunctionType::Create(ir_parameter_list, ir_return_type));
        };

        auto template_struct_function_type = TemplateStruct::Create();
        template_struct_function_type->template_struct_impl = template_function_type;
        return template_struct_function_type;
    }

    std::shared_ptr<Template> CreateBitCastTemplate() {
        auto template_bit_cast = Template::Create();
        template_bit_cast->generator = [symbol_table = this->ir_builder->symbol_table,
                                        logger = this->logger,
                                        this](std::list<Symbol> symbol_template_arguments,
                                              std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 2) {
                logger->Error("should input 2 template argument");
            }
            auto ir_source_type = SymbolGet<ir::Type>(symbol_template_arguments.front());
            auto ir_target_type = SymbolGet<ir::Type>(symbol_template_arguments.back());
            if (!ir_source_type || !ir_target_type) {
                logger->Error("template arguments should be type");
            }
            if (ir_source_type->bytes != ir_target_type->bytes) {
                logger->Error("the types byte size are not same");
            }

            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_function_type = ir::FunctionType::Create({ir_source_type}, ir_target_type);
            auto ir_function = ir_tmp_builder->CreateFunction(
                "bit_cast" + GetTemplateArgumentsPostify(symbol_template_arguments),
                ir_function_type);
            ir_function->annotation_dict["inline"];

            ir_tmp_builder->CreateTopBlockForFunction(ir_function);
            ir_tmp_builder->Create<ir::Return>(ir_tmp_builder->Create<ir::BitCast>(
                ir_function->parameters.front(), ir_target_type));
            return ir_function;
        };

        return template_bit_cast;
    }

    std::shared_ptr<Template> CreateAsCastTemplate() {
        auto template_as_cast = Template::Create();
        template_as_cast->generator = [symbol_table = this->ir_builder->symbol_table,
                                       logger = this->logger,
                                       this](std::list<Symbol> symbol_template_arguments,
                                             std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 2) {
                logger->Error("should input 2 template argument");
            }
            auto ir_value_type = SymbolGet<ir::Type>(symbol_template_arguments.front());
            if (!ir_value_type) {
                logger->Error("the first template argument should be type");
            }
            auto ir_interface_prototype =
                SymbolGet<ir::InterfacePrototype>(symbol_template_arguments.back());
            if (!ir_interface_prototype) {
                logger->Error("the second template argument should be a interface prototype");
            }

            auto iter_interface = std::find_if(RANGE(ir_value_type->interface_dict), [=](auto x) {
                if (!x.second) return false;
                return x.second->prototype == ir_interface_prototype;
            });
            if (iter_interface == ir_value_type->interface_dict.end()) {
                logger->Error(fmt::format("the interface {} is not implemented",
                                          ir_interface_prototype->fullname));
            }

            auto ir_interface = iter_interface->second;
            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_pointer_type = ir_tmp_builder->GetManagedPtrType(ir_value_type);
            auto ir_function_type =
                ir::FunctionType::Create({ir_pointer_type}, ir_interface_prototype->dynamic_type);
            auto ir_function = ir_tmp_builder->CreateFunction(
                "__as" + GetTemplateArgumentsPostify(symbol_template_arguments), ir_function_type);
            ir_function->annotation_dict["inline"];

            ir_tmp_builder->CreateTopBlockForFunction(ir_function);
            ir_tmp_builder->Create<ir::Return>(ir_tmp_builder->Call(
                ir_interface->dynamic_type_creator, ListCast<ir::Value>(ir_function->parameters)));
            return ir_function;
        };

        return template_as_cast;
    }

    std::shared_ptr<Template> CreateDynamicCastTemplate() {
        auto template_dynamic_cast = Template::Create();
        template_dynamic_cast->generator = [symbol_table = this->ir_builder->symbol_table,
                                            logger = this->logger,
                                            this](std::list<Symbol> symbol_template_arguments,
                                                  std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 2) {
                logger->Error("should input 2 template argument");
            }

            auto ir_interface_prototype =
                SymbolGet<ir::InterfacePrototype>(symbol_template_arguments.front());
            if (!ir_interface_prototype) {
                logger->Error("the first template argument should be a interface prototype");
            }
            auto ir_target_value_type = SymbolGet<ir::Type>(symbol_template_arguments.back());
            if (!ir_target_value_type) {
                logger->Error("the second template argument should be type");
            }

            auto iter_interface_implement = std::find_if(
                RANGE(ir_target_value_type->interface_dict),
                [=](auto x) { return x.second && x.second->prototype == ir_interface_prototype; });
            if (iter_interface_implement == ir_target_value_type->interface_dict.end()) {
                logger->Error("invalid dynamic Cast, operand is not a interface dynamic type");
            }
            auto ir_interface_implement = iter_interface_implement->second;
            // interface implement必然有一个函数, 我们通过该函数来判断dynamic
            // type是否是某一个具体类型
            PRAJNA_ASSERT(ir_interface_implement->functions.front());

            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_pointer_type = ir_tmp_builder->GetManagedPtrType(ir_target_value_type);
            auto ir_function_type =
                ir::FunctionType::Create({ir_interface_prototype->dynamic_type}, ir_pointer_type);
            auto ir_function = ir_tmp_builder->CreateFunction(
                "__dynamic_cast" + GetTemplateArgumentsPostify(symbol_template_arguments),
                ir_function_type);
            ir_function->annotation_dict["inline"];

            ir_tmp_builder->CreateTopBlockForFunction(ir_function);

            auto ir_target_ptr_type = ir_tmp_builder->GetManagedPtrType(ir_target_value_type);
            auto ir_ptr = ir_tmp_builder->Create<ir::LocalVariable>(ir_target_ptr_type);
            auto ir_interface_implement_function0 =
                ir_interface_implement
                    ->undef_this_pointer_functions[ir_interface_implement->functions.front()];

            auto template_cast =
                SymbolGet<Template>(ir_tmp_builder->GetSymbolByPath(true, {"cast"}));
            auto ir_rawptr_to_i64_cast_function = SymbolGet<ir::Value>(template_cast->Instantiate(
                {ir_interface_implement_function0->type, ir_tmp_builder->GetInt64Type()},
                ir_tmp_builder->module));
            auto ir_rawptr_i64_0 = ir_tmp_builder->Call(ir_rawptr_to_i64_cast_function,
                                                        ir_interface_implement_function0);

            auto ir_dynamic_object = ir_function->parameters.front();
            auto ir_rawptr_i64_1 = ir_tmp_builder->Call(
                ir_rawptr_to_i64_cast_function,
                ir_tmp_builder->AccessField(
                    ir_dynamic_object, ir_interface_implement->functions.front()->name + "/fp"));
            auto ir_condition =
                ir_tmp_builder->CallBinaryOperator(ir_rawptr_i64_0, "==", ir_rawptr_i64_1);
            auto ir_if = ir_tmp_builder->Create<ir::If>(ir_condition, ir::Block::Create(),
                                                        ir::Block::Create());
            ir_if->TrueBlock()->parent = ir_tmp_builder->function_stack.top();
            ir_if->FalseBlock()->parent = ir_tmp_builder->function_stack.top();

            ir_tmp_builder->PushBlock(ir_if->TrueBlock());
            ir_tmp_builder->Create<ir::WriteVariableLiked>(
                ir_tmp_builder->Call(
                    ir_tmp_builder->GetMemberFunction(ir_target_ptr_type, "FromUndef"),
                    ir_tmp_builder->AccessField(ir_dynamic_object, "object_pointer")),
                ir_ptr);
            ir_tmp_builder->PopBlock();

            ir_tmp_builder->PushBlock(ir_if->FalseBlock());
            ir_tmp_builder->ExitWithPrintErrorMessage("invalid cast dynamic cast");
            auto ir_nullptr =
                ir_tmp_builder->Call(ir_tmp_builder->GetMemberFunction(ir_ptr->type, "Null"));
            ir_tmp_builder->Create<ir::WriteVariableLiked>(ir_nullptr, ir_ptr);
            ir_tmp_builder->PopBlock();
            ir_tmp_builder->Create<ir::Return>(ir_ptr);

            return ir_function;
        };

        return template_dynamic_cast;
    }

    std::shared_ptr<Template> CreateDynamicIsTemplate() {
        auto template_dynamic_is = Template::Create();
        template_dynamic_is->generator = [symbol_table = this->ir_builder->symbol_table,
                                          logger = this->logger,
                                          this](std::list<Symbol> symbol_template_arguments,
                                                std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 2) {
                logger->Error("should input 2 template argument");
            }

            auto ir_interface_prototype =
                SymbolGet<ir::InterfacePrototype>(symbol_template_arguments.front());
            if (!ir_interface_prototype) {
                logger->Error("the first template argument should be a interface prototype");
            }
            auto ir_target_value_type = SymbolGet<ir::Type>(symbol_template_arguments.back());
            if (!ir_target_value_type) {
                logger->Error("the second template argument should be type");
            }

            auto iter_interface_implement = std::find_if(
                RANGE(ir_target_value_type->interface_dict),
                [=](auto x) { return x.second && x.second->prototype == ir_interface_prototype; });
            if (iter_interface_implement == ir_target_value_type->interface_dict.end()) {
                logger->Error("invalid dynamic Cast, operand is not a interface dynamic type");
            }
            auto ir_interface_implement = iter_interface_implement->second;
            // interface implement必然有一个函数, 我们通过该函数来判断dynamic
            // type是否是某一个具体类型
            PRAJNA_ASSERT(ir_interface_implement->functions.front());

            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_pointer_type = ir_tmp_builder->GetManagedPtrType(ir_target_value_type);
            auto ir_function_type = ir::FunctionType::Create({ir_interface_prototype->dynamic_type},
                                                             ir::BoolType::Create());
            auto ir_function = ir_tmp_builder->CreateFunction(
                "__dynamic_is" + GetTemplateArgumentsPostify(symbol_template_arguments),
                ir_function_type);
            ir_function->annotation_dict["inline"];

            ir_tmp_builder->CreateTopBlockForFunction(ir_function);

            auto ir_target_ptr_type = ir_tmp_builder->GetManagedPtrType(ir_target_value_type);
            auto ir_ptr = ir_tmp_builder->Create<ir::LocalVariable>(ir_target_ptr_type);
            auto ir_interface_implement_function0 =
                ir_interface_implement
                    ->undef_this_pointer_functions[ir_interface_implement->functions.front()];

            auto template_cast =
                SymbolGet<Template>(ir_tmp_builder->GetSymbolByPath(true, {"cast"}));
            auto ir_rawptr_to_i64_cast_function = SymbolGet<ir::Value>(template_cast->Instantiate(
                {ir_interface_implement_function0->type, ir_tmp_builder->GetInt64Type()},
                ir_tmp_builder->module));
            auto ir_rawptr_i64_0 = ir_tmp_builder->Call(ir_rawptr_to_i64_cast_function,
                                                        ir_interface_implement_function0);

            auto ir_dynamic_object = ir_function->parameters.front();
            auto ir_rawptr_i64_1 = ir_tmp_builder->Call(
                ir_rawptr_to_i64_cast_function,
                ir_tmp_builder->AccessField(
                    ir_dynamic_object, ir_interface_implement->functions.front()->name + "/fp"));
            auto ir_condition =
                ir_tmp_builder->CallBinaryOperator(ir_rawptr_i64_0, "==", ir_rawptr_i64_1);
            ir_tmp_builder->Create<ir::Return>(ir_condition);

            return ir_function;
        };

        return template_dynamic_is;
    }

    std::shared_ptr<Template> CreateSizeOfTemplate() {
        auto template_sizeof = Template::Create();
        template_sizeof->generator = [symbol_table = this->ir_builder->symbol_table,
                                      logger = this->logger,
                                      this](std::list<Symbol> symbol_template_arguments,
                                            std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 1) {
                logger->Error("should input 1 template argument");
            }
            auto ir_type = SymbolGet<ir::Type>(symbol_template_arguments.front());
            if (!ir_type) {
                logger->Error("template argument should be type");
            }

            // 的确存在为零的情况, 比如没有字段的结构
            // PRAJNA_ASSERT(ir_type->bytes > 0);
            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_function_type = ir::FunctionType::Create({}, ir_builder->GetInt64Type());
            auto ir_function = ir_tmp_builder->CreateFunction(
                "sizeof" + GetTemplateArgumentsPostify(symbol_template_arguments),
                ir_function_type);
            ir_function->annotation_dict["inline"];

            ir_tmp_builder->CreateTopBlockForFunction(ir_function);
            ir_tmp_builder->Create<ir::Return>(ir_tmp_builder->GetInt64Constant(ir_type->bytes));
            return ir_function;
        };

        return template_sizeof;
    }

    std::shared_ptr<Template> CreateCompareInstructionTemplate(std::string compare_operation_name) {
        auto template_compare = Template::Create();

        template_compare->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                       logger = this->logger](
                                          std::list<Symbol> symbol_template_arguments,
                                          std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 1) {
                logger->Error("should input 1 template argument");
            }
            auto ir_type = SymbolGet<ir::Type>(symbol_template_arguments.front());
            if (!ir_type) {
                logger->Error("template argument should be a type");
            }

            auto compare_operation = ir::CompareInstruction::Operation::None;
            // arithmatic
            if (compare_operation_name == "eq") {
                if (Is<ir::IntType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::ICMP_EQ;
                } else {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_OEQ;
                }
            }
            if (compare_operation_name == "ne") {
                if (Is<ir::IntType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::ICMP_NE;
                } else {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_ONE;
                }
            }
            if (compare_operation_name == "gt") {
                if (auto ir_int_type = Cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_SGT;
                    } else {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_UGT;
                    }
                }
                if (Is<ir::FloatType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_OGT;
                }
            }
            if (compare_operation_name == "ge") {
                if (auto ir_int_type = Cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_SGE;
                    } else {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_UGE;
                    }
                }
                if (Is<ir::FloatType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_OGE;
                }
            }
            if (compare_operation_name == "lt") {
                if (auto ir_int_type = Cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_SLT;
                    } else {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_ULT;
                    }
                }
                if (Is<ir::FloatType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_OLT;
                }
            }
            if (compare_operation_name == "le") {
                if (auto ir_int_type = Cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_SLE;
                    } else {
                        compare_operation = ir::CompareInstruction::Operation::ICMP_ULE;
                    }
                }
                if (Is<ir::FloatType>(ir_type)) {
                    compare_operation = ir::CompareInstruction::Operation::FCMP_OLE;
                }
            }

            if (compare_operation == ir::CompareInstruction::Operation::None) {
                logger->Error("not support compare operation");
            }

            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_function_type =
                ir::FunctionType::Create({ir_type, ir_type}, ir::BoolType::Create());
            auto ir_function = ir_tmp_builder->CreateFunction(
                "__" + compare_operation_name +
                    GetTemplateArgumentsPostify(symbol_template_arguments),
                ir_function_type);
            ir_function->annotation_dict["inline"];
            ir_tmp_builder->CreateTopBlockForFunction(ir_function);
            ir_tmp_builder->Create<ir::Return>(ir_tmp_builder->Create<ir::CompareInstruction>(
                compare_operation, ir_function->parameters.front(),
                ir_function->parameters.back()));
            return ir_function;
        };

        return template_compare;
    }

    std::shared_ptr<Template> CreateBinaryOperatorTemplate(std::string binary_operator_name) {
        auto template_binary_operator = Template::Create();

        template_binary_operator->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                               logger = this->logger](
                                                  std::list<Symbol> symbol_template_arguments,
                                                  std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 1) {
                logger->Error("should input 1 template argument");
            }
            auto ir_type = SymbolGet<ir::Type>(symbol_template_arguments.front());
            if (!ir_type) {
                logger->Error("the template argument should be a type");
            }

            auto binary_operation = ir::BinaryOperator::Operation::None;
            // arithmatic
            if (binary_operator_name == "add") {
                if (Is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Add;
                }
                if (Is<ir::FloatType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::FAdd;
                }
            }
            if (binary_operator_name == "sub") {
                if (Is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Sub;
                }
                if (Is<ir::FloatType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::FSub;
                }
            }
            if (binary_operator_name == "mul") {
                if (Is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Mul;
                }
                if (Is<ir::FloatType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::FMul;
                }
            }
            if (binary_operator_name == "div") {
                if (auto ir_int_type = Cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        binary_operation = ir::BinaryOperator::Operation::SDiv;
                    } else {
                        binary_operation = ir::BinaryOperator::Operation::UDiv;
                    }
                }
                if (Is<ir::FloatType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::FDiv;
                }
            }
            if (binary_operator_name == "rem") {
                if (auto ir_int_type = Cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        binary_operation = ir::BinaryOperator::Operation::SRem;
                    } else {
                        binary_operation = ir::BinaryOperator::Operation::URem;
                    }
                }
                if (Is<ir::FloatType>(ir_type)) {
                    auto ir_float_type = Cast<ir::FloatType>(ir_type);
                    binary_operation = ir::BinaryOperator::Operation::FRem;
                }
            }
            // logical
            if (binary_operator_name == "and") {
                if (Is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::And;
                }
            }
            if (binary_operator_name == "or") {
                if (Is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Or;
                }
            }
            if (binary_operator_name == "xor") {
                if (Is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Xor;
                }
            }

            // shift
            if (binary_operator_name == "shift_left") {
                if (Is<ir::IntType>(ir_type)) {
                    binary_operation = ir::BinaryOperator::Operation::Shl;
                }
            }
            if (binary_operator_name == "shift_right") {
                if (auto ir_int_type = Cast<ir::IntType>(ir_type)) {
                    if (ir_int_type->is_signed) {
                        binary_operation = ir::BinaryOperator::Operation::AShr;
                    } else {
                        binary_operation = ir::BinaryOperator::Operation::LShr;
                    }
                }
            }

            if (binary_operation == ir::BinaryOperator::Operation::None) {
                logger->Error("not support binary operator");
            }

            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_function_type = ir::FunctionType::Create({ir_type, ir_type}, ir_type);
            auto ir_function = ir_tmp_builder->CreateFunction(
                "__" + binary_operator_name +
                    GetTemplateArgumentsPostify(symbol_template_arguments),
                ir_function_type);
            ir_function->annotation_dict["inline"];
            ir_tmp_builder->CreateTopBlockForFunction(ir_function);
            ir_tmp_builder->Create<ir::Return>(ir_tmp_builder->Create<ir::BinaryOperator>(
                binary_operation, ir_function->parameters.front(), ir_function->parameters.back()));
            return ir_function;
        };

        return template_binary_operator;
    }

    std::shared_ptr<Template> CreateFloatSmallestOrLargest(
        ir::ConstantFloat::SpecialValue special_value, bool is_negative) {
        auto template_binary_operator = Template::Create();

        template_binary_operator->generator = [=, symbol_table = this->ir_builder->symbol_table,
                                               logger = this->logger](
                                                  std::list<Symbol> symbol_template_arguments,
                                                  std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 1) {
                logger->Error("should input 1 template argument");
            }
            auto ir_float_type =
                Cast<ir::FloatType>(SymbolGet<ir::Type>(symbol_template_arguments.front()));
            if (!ir_float_type) {
                logger->Error("template argument should be a float type");
            }

            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_function_type = ir::FunctionType::Create({}, ir_float_type);
            std::string ir_function_name_prefix;
            switch (special_value) {
                case ir::ConstantFloat::SpecialValue::Smallest:
                    ir_function_name_prefix = "__smallest";
                    break;
                case ir::ConstantFloat::SpecialValue::Largest:
                    ir_function_name_prefix = "__largest";
                    break;
                case ir::ConstantFloat::SpecialValue::NaN:
                    ir_function_name_prefix = "__nan";
                    break;
                case ir::ConstantFloat::SpecialValue::Inf:
                    ir_function_name_prefix = "__inf";
                    break;
                default:
                    PRAJNA_UNREACHABLE;
                    break;
            }
            auto is_neg_name = is_negative ? std::string("_negative") : "";
            auto ir_function = ir_tmp_builder->CreateFunction(
                ir_function_name_prefix + is_neg_name +
                    GetTemplateArgumentsPostify(symbol_template_arguments),
                ir_function_type);
            ir_function->annotation_dict["inline"];

            ir_tmp_builder->CreateTopBlockForFunction(ir_function);
            auto ir_constant_float = ir_tmp_builder->Create<ir::ConstantFloat>(ir_float_type, 0.0);
            ir_constant_float->special_value = special_value;
            ir_constant_float->is_negative = is_negative;
            ir_tmp_builder->Create<ir::Return>(ir_constant_float);
            return ir_function;
        };

        return template_binary_operator;
    }

    std::shared_ptr<Template> CreateInitializeTemplate() {
        auto template_initialize = Template::Create();
        template_initialize->generator = [symbol_table = this->ir_builder->symbol_table,
                                          logger = this->logger,
                                          this](std::list<Symbol> symbol_template_arguments,
                                                std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 1) {
                logger->Error("should input 1 template argument");
            }
            auto ir_type = SymbolGet<ir::Type>(symbol_template_arguments.front());
            if (!ir_type) {
                logger->Error("template arguments should be type");
            }

            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_function_type = ir::FunctionType::Create({ir::PointerType::Create(ir_type)},
                                                             ir::VoidType::Create());
            auto ir_function = ir_tmp_builder->CreateFunction(
                "initialize" + GetTemplateArgumentsPostify(symbol_template_arguments),
                ir_function_type);
            ir_function->annotation_dict["inline"];
            ir_tmp_builder->CreateTopBlockForFunction(ir_function);
            // 标记新的IR, 防止递归初始化
            ir_tmp_builder->create_callback = [](std::shared_ptr<ir::Value> ir_value) {
                ir_value->annotation_dict["INSERTED_FLAG"].push_back("none");
            };

            InitializeVariableLikedCallback(
                ir_tmp_builder->Create<ir::DeferencePointer>(ir_function->parameters.front()),
                ir_tmp_builder);
            ir_tmp_builder->Create<ir::Return>(ir_tmp_builder->Create<ir::VoidValue>());

            return ir_function;
        };

        return template_initialize;
    }

    std::shared_ptr<Template> CreateDestroyTemplate() {
        auto template_destroy = Template::Create();
        template_destroy->generator = [symbol_table = this->ir_builder->symbol_table,
                                       logger = this->logger,
                                       this](std::list<Symbol> symbol_template_arguments,
                                             std::shared_ptr<ir::Module> ir_module) -> Symbol {
            if (symbol_template_arguments.size() != 1) {
                logger->Error("should input 1 template argument");
            }
            auto ir_type = SymbolGet<ir::Type>(symbol_template_arguments.front());
            if (!ir_type) {
                logger->Error("template arguments should be type");
            }

            auto ir_tmp_builder = IrBuilder::Create(symbol_table, ir_module, logger);
            auto ir_function_type = ir::FunctionType::Create({ir::PointerType::Create(ir_type)},
                                                             ir::VoidType::Create());
            auto ir_function = ir_tmp_builder->CreateFunction(
                "finalize" + GetTemplateArgumentsPostify(symbol_template_arguments),
                ir_function_type);
            ir_function->annotation_dict["inline"];
            ir_tmp_builder->CreateTopBlockForFunction(ir_function);
            // 标记新的IR, 防止递归初始化
            ir_tmp_builder->create_callback = [](std::shared_ptr<ir::Value> ir_value) {
                ir_value->annotation_dict["INSERTED_FLAG"].push_back("none");
            };

            auto ir_object =
                ir_tmp_builder->Create<ir::DeferencePointer>(ir_function->parameters.front());
            FinalizeVariableLikedCallback(ir_object, ir_tmp_builder);
            ir_tmp_builder->Create<ir::Return>(ir_tmp_builder->Create<ir::VoidValue>());

            return ir_function;
        };

        return template_destroy;
    }

    void Stage0() {
        auto bool_type = ir::BoolType::Create();
        auto char_type = ir::CharType::Create();
        auto void_type = ir::VoidType::Create();
        auto undef_type = ir::UndefType::Create();
        ir_builder->symbol_table->Set(bool_type, bool_type->name);
        ir_builder->symbol_table->Set(char_type, char_type->name);
        ir_builder->symbol_table->Set(void_type, void_type->name);
        ir_builder->symbol_table->Set(undef_type, undef_type->name);
    }

    void Stage1() {
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBitCastTemplate(), "bit_cast");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateSizeOfTemplate(), "sizeof");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateIntTypeTemplate(true), "Int");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateIntTypeTemplate(false), "Uint");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeTemplate(), "Float");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateRawArrayTypeTemplate(), "array");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateRawPtrTypeTemplate(), "ptr");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFunctionTypePointerTemplate(), "function");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateAsCastTemplate(), "__as");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateDynamicCastTemplate(), "__dynamic_cast");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateDynamicIsTemplate(), "__dynamic_is");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateCastInstructionTemplate(), "cast");
        // arithematic
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBinaryOperatorTemplate("add"), "__add");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBinaryOperatorTemplate("sub"), "__sub");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBinaryOperatorTemplate("mul"), "__mul");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBinaryOperatorTemplate("div"), "__div");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBinaryOperatorTemplate("rem"), "__rem");
        // logical
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBinaryOperatorTemplate("and"), "__and");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBinaryOperatorTemplate("or"), "__or");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBinaryOperatorTemplate("xor"), "__xor");
        // shift
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBinaryOperatorTemplate("shift_left"), "__shift_left");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateBinaryOperatorTemplate("shift_right"), "__shift_right");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateCompareInstructionTemplate("eq"), "__eq");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateCompareInstructionTemplate("ne"), "__ne");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateCompareInstructionTemplate("gt"), "__gt");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateCompareInstructionTemplate("ge"), "__ge");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateCompareInstructionTemplate("lt"), "__lt");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateCompareInstructionTemplate("le"), "__le");
        //
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("sin"), "__sin");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("cos"), "__cos");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("sqrt"), "__sqrt");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("pow", 2), "__pow");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("exp", 2), "__exp");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("exp2", 2), "__exp2");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("log", 2), "__log");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("log10", 2), "__log10");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("log2", 2), "__log2");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("fabs"), "__fabs");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("floor"), "__floor");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("ceil"), "__ceil");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("trunc"), "__trunc");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("rint"), "__rint");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("nearby"), "__nearby");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("round"), "__round");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatTypeIntrinsicUnaryFunctionTemplate("roundeven"), "__roundeven");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatSmallestOrLargest(ir::ConstantFloat::SpecialValue::Smallest, false),
            "__smallest");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatSmallestOrLargest(ir::ConstantFloat::SpecialValue::Smallest, true),
            "__smallest_negative");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatSmallestOrLargest(ir::ConstantFloat::SpecialValue::Largest, false),
            "__largest");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatSmallestOrLargest(ir::ConstantFloat::SpecialValue::Largest, true),
            "__largest_negative");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatSmallestOrLargest(ir::ConstantFloat::SpecialValue::NaN, false),
            "__nan");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatSmallestOrLargest(ir::ConstantFloat::SpecialValue::NaN, true),
            "__nan_negative");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatSmallestOrLargest(ir::ConstantFloat::SpecialValue::Inf, false),
            "__inf");
        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateFloatSmallestOrLargest(ir::ConstantFloat::SpecialValue::Inf, true),
            "__inf_negative");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            expression_lowering_visitor->CreateDynamicTemplate(), "Dynamic");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateInitializeTemplate(), "initialize");

        ir_builder->symbol_table->RootSymbolTable()->SetWithAssigningName(
            this->CreateDestroyTemplate(), "finalize");
    }

    Symbol operator()(ast::Pragma ast_pragma);

    Symbol operator()(ast::InterfacePrototype ast_interface_prototype) {
        auto ir_interface_prototype = ir::InterfacePrototype::Create();
        ir_builder->SetSymbolWithTemplateArgumentsPostify(ir_interface_prototype,
                                                          ast_interface_prototype.name);

        ir_builder->interface_prototype_processing = true;
        auto guard =
            ScopeExit::Create([=]() { ir_builder->interface_prototype_processing = false; });

        ir_interface_prototype->dynamic_type = ir::StructType::Create({});
        // 我们通过dynamic<Interface>来获取接口所对应的类型, 需要将他们关联,
        // 以便获取dynamic的实现
        auto template_struct_dynamic =
            SymbolGet<TemplateStruct>(ir_builder->GetSymbolByPath(true, {"Dynamic"}));
        PRAJNA_ASSERT(template_struct_dynamic);
        ir_interface_prototype->dynamic_type->template_struct = template_struct_dynamic;

        for (auto ast_function_declaration : ast_interface_prototype.functions) {
            ir_builder->PushSymbolTable();
            auto guard = ScopeExit::Create([=]() { this->ir_builder->PopSymbolTable(); });
            ir_builder->symbol_table->name = ir_interface_prototype->name;
            auto symbol_function = (*this)(ast_function_declaration);
            auto ir_function = Cast<ir::Function>(SymbolGet<ir::Value>(symbol_function));
            ir_interface_prototype->functions.push_back(ir_function);
            // 需要将去从module移出来, 这里的function并不会实际被生成
            ir_builder->module->functions.remove(ir_function);
        }

        this->CreateInterfaceDynamicType(ir_interface_prototype);
        // 用到的时候再进行该操作, 因为很多原生接口实现时候ptr还未
        return ir_interface_prototype;
    }

    void CreateInterfaceDynamicType(
        std::shared_ptr<ir::InterfacePrototype> ir_interface_prototype) {
        auto field_object_pointer = ir::Field::Create(
            "object_pointer", ir_builder->GetManagedPtrType(ir::UndefType::Create()));
        auto ir_interface_struct = ir_interface_prototype->dynamic_type;
        ir_interface_struct->fields.push_back(field_object_pointer);
        ir_interface_struct->name = ir_interface_prototype->name;
        ir_interface_struct->fullname = ir_interface_prototype->fullname;

        PRAJNA_ASSERT(!ir_builder->current_implement_type);
        ir_builder->current_implement_type = ir_interface_struct;

        auto ir_interface = ir::InterfaceImplement::Create();
        for (auto ir_function : ir_interface_prototype->functions) {
            auto ir_callee_argument_types = ir_function->function_type->parameter_types;
            ir_callee_argument_types.insert(ir_callee_argument_types.begin(),
                                            ir::PointerType::Create(ir::UndefType::Create()));
            auto ir_callee_type = ir::FunctionType::Create(ir_callee_argument_types,
                                                           ir_function->function_type->return_type);

            auto field_function_pointer = ir::Field::Create(
                ir_function->name + "/fp", ir::PointerType::Create(ir_callee_type));
            ir_interface_struct->fields.push_back(field_function_pointer);
            ir_interface_struct->Update();

            auto ir_member_function_argument_types = ir_function->function_type->parameter_types;
            // 必然有一个this pointer参数
            // PRAJNA_ASSERT(ir_member_function_argument_types.size() >= 1);
            // 第一个参数应该是this pointer的类型, 修改一下
            ir_member_function_argument_types.insert(
                ir_member_function_argument_types.begin(),
                ir::PointerType::Create(ir_builder->current_implement_type));
            auto ir_member_function_type = ir::FunctionType::Create(
                ir_member_function_argument_types, ir_function->function_type->return_type);
            auto ir_member_function =
                ir_builder->CreateFunction(ir_function->name + "/member", ir_member_function_type);
            ir_member_function->name = ir_function->name;
            ir_member_function->fullname = ir_function->fullname;

            // 这里还是使用类型IrBuilder
            ir_builder->CreateTopBlockForFunction(ir_member_function);

            auto ir_this_pointer = ir_member_function->parameters.front();
            // 这里叫函数指针, 函数的类型就是函数指针
            auto ir_function_pointer = ir_builder->Create<ir::AccessField>(
                ir_builder->Create<ir::DeferencePointer>(ir_this_pointer), field_function_pointer);
            // 直接将外层函数的参数转发进去, 除了第一个参数需要调整一下
            std::list<std::shared_ptr<ir::Value>> ir_arguments;
            std::transform(RANGE(ir_member_function->parameters), std::back_inserter(ir_arguments),
                           [](auto x) -> std::shared_ptr<ir::Value> { return x; });
            ir_arguments.front() = ir_builder->AccessField(
                ir_builder->Create<ir::AccessField>(
                    ir_builder->Create<ir::DeferencePointer>(ir_this_pointer),
                    field_object_pointer),
                "raw_ptr");
            auto ir_function_call = ir_builder->Call(ir_function_pointer, ir_arguments);
            if (!Is<ir::VoidType>(ir_function_call->type)) {
                ir_builder->Create<ir::Return>(ir_function_call);
            } else {
                ir_builder->Create<ir::Return>(ir_builder->Create<ir::VoidValue>());
            }

            ir_interface->functions.push_back(ir_member_function);
        }

        ir_builder->current_implement_type = nullptr;

        ir_interface_struct->interface_dict["Self"] = ir_interface;
    }

    Symbol operator()(ast::Blank) { return nullptr; }

    template <typename Statement_>
    Symbol operator()(Statement_ ast_statement) {
        PRAJNA_ASSERT(false, typeid(ast_statement).name());
        return nullptr;
    }

    std::shared_ptr<ir::Value> ApplyExpression(ast::Expression ast_expression) {
        return expression_lowering_visitor->Apply(ast_expression);
    }

   public:
    std::shared_ptr<ExpressionLoweringVisitor> expression_lowering_visitor = nullptr;
    std::shared_ptr<Compiler> compiler = nullptr;
    std::shared_ptr<IrBuilder> ir_builder = nullptr;
    std::shared_ptr<Logger> logger = nullptr;
};

}  // namespace prajna::lowering
