#pragma once

#include <map>
#include <memory>
#include <stack>

#include "prajna/ir/ir.hpp"
#include "prajna/logger.hpp"
#include "prajna/lowering/statement_lowering_visitor.hpp"
#include "prajna/parser/parse.h"
#include "prajna/transform/extract_gpu_grid_pass.hpp"
#include "prajna/transform/flattern_block.hpp"
#include "prajna/transform/inline_function.hpp"
#include "prajna/transform/reference_count.hpp"
#include "prajna/transform/transform_pass.hpp"
#include "prajna/transform/utility.hpp"

namespace prajna::ir {
class Module;
}

namespace prajna::ast {
class Statements;
}

namespace prajna::transform {

template <typename Func>
inline bool RecursiveTransformModule(std::shared_ptr<ir::Module> ir_module, Func fun) {
    auto re = fun(ir_module);

    for (auto [ir_target, ir_sub_module] : ir_module->modules) {
        if (!ir_sub_module) continue;

        fun(ir_sub_module);
    }

    return re;
}

std::shared_ptr<ir::Module> ConvertVariableToPointer(std::shared_ptr<ir::Module> ir_module);

std::shared_ptr<ir::Module> MakeCompatiableWithLlvm(std::shared_ptr<ir::Module> ir_module);

std::shared_ptr<ir::Module> SperateModule(std::shared_ptr<ir::Module> ir_module);

inline bool ConvertPropertyToFunctionCall(std::shared_ptr<ir::Module> ir_module) {
    auto ir_access_properties = utility::GetAll<ir::AccessProperty>(ir_module);
    for (auto ir_access_property : ir_access_properties) {
        auto ir_block = ir_access_property->GetParentBlock();
        auto ir_builder = lowering::IrBuilder::Create();
        auto scope = ir_builder->PushBlockRAII(ir_block);
        ir_builder->inserter_iterator = std::ranges::find(*ir_block, ir_access_property);

        for (auto instruction_with_index : Clone(ir_access_property->instruction_with_index_list)) {
            auto ir_inst = Lock(instruction_with_index.instruction);
            int64_t op_idx = instruction_with_index.operand_index;

            PRAJNA_ASSERT(!Is<ir::GetAddressOfVariableLiked>(ir_inst));

            if (Is<ir::WriteProperty>(ir_inst) && op_idx == 1) {
                auto ir_write_property = Cast<ir::WriteProperty>(ir_inst);
                auto ir_arguments = ir_access_property->Arguments();
                ir_arguments.insert(ir_arguments.begin(), ir_access_property->ThisPointer());
                ir_arguments.push_back(ir_write_property->Value());
                // 需要在write操作前插入
                auto ir_tmp_builder = lowering::IrBuilder::Create();

                {
                    auto scope = ir_tmp_builder->PushBlockRAII(ir_block);
                    ir_tmp_builder->inserter_iterator =
                        std::ranges::find(*ir_block, ir_write_property);
                    auto ir_setter_call = ir_tmp_builder->Create<ir::Call>(
                        ir_access_property->property->set_function, ir_arguments);
                    utility::RemoveFromParent(ir_write_property);
                    ir_write_property->Finalize();
                }

            } else {
                auto ir_arguments = ir_access_property->Arguments();
                ir_arguments.insert(ir_arguments.begin(), ir_access_property->ThisPointer());
                auto ir_getter_call = ir_builder->Create<ir::Call>(
                    ir_access_property->property->get_function, ir_arguments);
                ir_inst->SetOperand(op_idx, ir_getter_call);
            }
        }

        bool unused = ir_access_property->instruction_with_index_list.empty();
        if (unused) {
            auto ir_arguments = ir_access_property->Arguments();
            ir_arguments.insert(ir_arguments.begin(), ir_access_property->ThisPointer());
            auto ir_getter_call =
                ir_builder->Call(ir_access_property->property->get_function, ir_arguments);
        }

        utility::RemoveFromParent(ir_access_property);
        ir_access_property->Finalize();
    }

    return !ir_access_properties.empty();
}

inline void ConvertKernelFunctionCallToKernelLaunch(std::shared_ptr<ir::Module> ir_module) {
    std::string runtime_namespace;
    // PRAJNA_ASSERT(!runtime_namespace.empty());
    for (auto ir_function : ir_module->functions) {
        auto ir_kernel_function_calls = utility::GetAll<ir::KernelFunctionCall>(ir_function);
        for (auto ir_kernel_function_call : ir_kernel_function_calls) {
            auto ir_kernel_function = ir_kernel_function_call->Function();
            auto ir_grid_shape = ir_kernel_function_call->GridShape();
            auto ir_block_shape = ir_kernel_function_call->BlockShape();
            // auto ir_arguments = ir_kernel_function_call->parameters();
            auto ir_block = ir_kernel_function_call->GetParentBlock();

            auto ir_builder =
                lowering::IrBuilder::Create(ir_module->symbol_table, ir_module, nullptr);
            auto scope = ir_builder->PushBlockRAII(ir_block);
            ir_builder->inserter_iterator = std::ranges::find(*ir_block, ir_kernel_function_call);

            // 构建::cuda::launchKernel的逻辑
            auto ir_kernel_arguments_address_array_i8ptr =
                ir_builder->Create<ir::LocalVariable>(ir::ArrayType::Create(
                    ir::PointerType::Create(ir::i8), ir_kernel_function_call->ArgumentSize()));
            for (int64_t i = 0; i < ir_kernel_function_call->ArgumentSize(); ++i) {
                auto ir_argument = ir_kernel_function_call->Argument(i);
                auto ir_kernel_argument_address_i8ptr = ir_builder->Create<ir::BitCast>(
                    ir_builder->Create<ir::GetAddressOfVariableLiked>(
                        ir_builder->VariableLikedNormalize(ir_argument)),
                    ir::PointerType::Create(ir::i8));
                auto ir_array_index = ir_builder->Create<ir::IndexArray>(
                    ir_kernel_arguments_address_array_i8ptr, ir_builder->GetConstant<int64_t>(i));
                auto ir_array_index_write = ir_builder->Create<ir::WriteVariableLiked>(
                    ir_kernel_argument_address_i8ptr, ir_array_index);
            }

            if (ir_kernel_function->annotation_dict["target"].front() == "nvptx") {
                runtime_namespace = "nvgpu";
            } else if (ir_kernel_function->annotation_dict["target"].front() == "amdgpu") {
                runtime_namespace = "gpu2";
            } else {
                PRAJNA_UNIMPLEMENT;
            }

            auto ir_launch_function = lowering::SymbolGet<ir::Value>(
                lowering::SymbolGet<lowering::SymbolTable>(
                    ir_module->symbol_table->RootSymbolTable()->Get(runtime_namespace))
                    ->Get("LaunchKernel"));
            PRAJNA_ASSERT(ir_launch_function);
            std::list<std::shared_ptr<ir::Value>> ir_arguments;
            ir_arguments.push_back(ir_builder->Create<ir::BitCast>(
                ir_kernel_function, ir::PointerType::Create(ir::i8)));
            ir_arguments.push_back(ir_grid_shape);
            ir_arguments.push_back(ir_block_shape);
            auto ir_array_index0 = ir_builder->Create<ir::IndexArray>(
                ir_kernel_arguments_address_array_i8ptr, ir_builder->GetConstant<int64_t>(0));
            auto ir_array_address = ir_builder->Create<ir::BitCast>(
                ir_builder->Create<ir::GetAddressOfVariableLiked>(ir_array_index0),
                ir::PointerType::Create(ir::PointerType::Create(ir::i8)));
            ir_arguments.push_back(ir_array_address);
            auto ir_kernel_call = ir_builder->Call(ir_launch_function, ir_arguments);
            auto ir_zero = ir_builder->GetConstant<int64_t>(0);
            auto ir_condition = ir_builder->CallBinaryOperator(ir_kernel_call, "!=", ir_zero);
            auto ir_if =
                ir_builder->Create<ir::If>(ir_condition, ir::Block::Create(), ir::Block::Create());

            {
                auto scope = ir_builder->PushBlockRAII(ir_if->TrueBlock());
                auto ir_str = ir_builder->GetString("Failed launch kernel: ret: ");
                ir_builder->CallMemberFunction(ir_str, "Print", {});
                ir_builder->CallMemberFunction(ir_kernel_call, "PrintLine", {});
            }

            utility::RemoveFromParent(ir_kernel_function_call);
            ir_kernel_function_call->Finalize();
        }
    }
}

inline void ConvertKernelFunctionOperandToAddress(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_instructions = utility::GetAll<ir::Instruction>(ir_function);

        for (auto ir_instruction : ir_instructions) {
            for (int64_t i = 0; i < ir_instruction->OperandSize(); ++i) {
                if (auto ir_function = Cast<ir::Function>(ir_instruction->GetOperand(i))) {
                    if (ir_function->annotation_dict.count("kernel")) {
                        auto global_variable_fullname = GetKernelFunctionAddressName(ir_function);

                        std::shared_ptr<ir::GlobalVariable> ir_global_variable = nullptr;
                        auto iter_global_variable =
                            std::ranges::find_if(ir_module->global_variables,
                                                 [=](std::shared_ptr<ir::GlobalVariable> x) {
                                                     return x->fullname == global_variable_fullname;
                                                 });
                        if (iter_global_variable != ir_module->global_variables.end()) {
                            ir_global_variable = *iter_global_variable;
                        } else {
                            ir_global_variable = ir::GlobalVariable::Create(ir_function->type);
                            ir_global_variable->name = global_variable_fullname;
                            ir_global_variable->fullname = ir_global_variable->name;
                            ir_global_variable->annotation_dict = ir_function->annotation_dict;
                            // 如果不是同一个module的, 则为external, 目前所有的nvptx
                            // IR都会迁移的使用的Module里去
                            // ir_global_variable->is_external =
                            //     ir_function->parent_module != ir_module;
                            ir_module->AddGlobalVariable(ir_global_variable);
                        }

                        ir_instruction->SetOperand(i, ir_global_variable);
                    }
                }
            }
        }
    }
}

inline void ConvertGlobalVariableToPointer(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_global_variable : ir_module->global_variables) {
        auto ir_global_alloca = ir::GlobalAlloca::Create(ir_global_variable->type);
        ir_global_alloca->name = ir_global_variable->name;
        ir_global_alloca->fullname = ir_global_variable->fullname;
        // ir_global_variable->is_external默认为false
        ir_global_alloca->is_external = ir_global_variable->is_external;
        ir_module->AddGlobalAlloca(ir_global_alloca);
    }

    ir_module->global_variables.clear();

    for (auto ir_function : ir_module->functions) {
        auto ir_instructions = utility::GetAll<ir::Instruction>(ir_function);
        for (auto ir_instruction : ir_instructions) {
            for (int64_t i = 0; i < ir_instruction->OperandSize(); ++i) {
                // @note 全局变量目前遵循如果使用其他module的则自身为external的原则
                if (auto ir_global_variable =
                        Cast<ir::GlobalVariable>(ir_instruction->GetOperand(i))) {
                    auto iter_global_alloca = std::ranges::find_if(
                        ir_module->global_allocas, [=](std::shared_ptr<ir::GlobalAlloca> x) {
                            return x->fullname == ir_global_variable->fullname;
                        });
                    std::shared_ptr<ir::GlobalAlloca> ir_global_alloca = nullptr;
                    if (iter_global_alloca != ir_module->global_allocas.end()) {
                        ir_global_alloca = *iter_global_alloca;
                    } else {
                        ir_global_alloca = ir::GlobalAlloca::Create(ir_global_variable->type);
                        ir_global_alloca->name = ir_global_variable->name;
                        ir_global_alloca->fullname = ir_global_variable->fullname;
                        ir_global_alloca->is_external = true;
                        ir_module->AddGlobalAlloca(ir_global_alloca);
                    }

                    auto ir_deference_pointer = ir::DeferencePointer::Create(ir_global_alloca);
                    auto ir_block = ir_instruction->GetParentBlock();
                    auto iter = std::ranges::find(*ir_block, ir_instruction);
                    ir_block->Insert(iter, ir_deference_pointer);
                    ir_instruction->SetOperand(i, ir_deference_pointer);
                }
            }
        }
    }
}

inline void CloneExternalDeviceValue(std::shared_ptr<ir::Module> ir_module, ir::Target target) {
    if (ir_module->modules.count(target) == 0) {
        return;
    }

    auto ir_target_module = ir_module->modules[target];
    if (!ir_target_module) return;

    ir_module->global_allocas.remove_if([=](auto ir_global_alloca) -> bool {
        if (ir_global_alloca->address_space == 3) {
            ir_target_module->AddGlobalAlloca(ir_global_alloca);
            return true;
        } else {
            return false;
        }
    });

    std::list<std::shared_ptr<ir::Function>> ir_kernel_function_list;
    std::ranges::copy_if(ir_target_module->functions, std::back_inserter(ir_kernel_function_list),
                         [](std::shared_ptr<ir::Function> ir_function) {
                             return ir_function->annotation_dict.count("kernel");
                         });

    auto function_cloner = ir::FunctionCloner::Create(ir_target_module, false);
    for (auto ir_kernel_function : ir_kernel_function_list) {
        // 会把生成的函数直接插入到module里
        auto ir_kernel_function_new =
            Cast<ir::Function>(function_cloner->Clone(ir_kernel_function));
        // 移除原来的核函数
        ir_target_module->functions.remove(ir_kernel_function);
        PRAJNA_ASSERT(ir::Verify(ir_kernel_function_new));
    }
}

inline void DefineKernelFunctionAddress(std::shared_ptr<ir::Module> ir_module, ir::Target target) {
    if (ir_module->modules.count(target) == 0) {
        return;
    }

    auto ir_target_module = ir_module->modules[target];
    if (!ir_target_module) return;
    for (auto ir_function : ir_target_module->functions) {
        if (ir_function->annotation_dict.count("kernel")) {
            auto global_variable_fullname = GetKernelFunctionAddressName(ir_function);
            auto iter_global_variable = std::ranges::find_if(
                ir_module->global_variables, [=](std::shared_ptr<ir::GlobalVariable> x) {
                    return x->fullname == global_variable_fullname;
                });
            if (iter_global_variable == ir_module->global_variables.end()) {
                auto ir_global_variable = ir::GlobalVariable::Create(ir_function->type);
                ir_global_variable->name = global_variable_fullname;
                ir_global_variable->fullname = ir_global_variable->name;
                ir_module->AddGlobalVariable(ir_global_variable);
            }
        }
    }
}

// @brief 移除return后面的指令. 若不移除, 会存在未知错误
inline void RemoveValuesAfterReturn(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        for (auto ir_block : ir_function->blocks) {
            auto iter_return = std::ranges::find_if(*ir_block, [](auto x) {
                return Is<ir::Return>(x) || Is<ir::internal::JumpBranch>(x) ||
                       Is<ir::internal::ConditionBranch>(x);
            });
            if (iter_return != ir_block->end()) {
                ir_block->erase(std::next(iter_return), ir_block->end());
            }
        }
    }
}

inline bool DeclareExternalFunction(std::shared_ptr<ir::Module> ir_module) {
    Each<ir::Instruction>(ir_module, [=](std::shared_ptr<ir::Instruction> ir_instruction) {
        for (int64_t i = 0; i < ir_instruction->OperandSize(); ++i) {
            auto ir_operand = ir_instruction->GetOperand(i);

            if (auto ir_function = Cast<ir::Function>(ir_operand)) {
                if (ir_function->GetParentModule() != ir_module) {
                    std::shared_ptr<ir::Function> ir_decl_function = nullptr;
                    auto iter_fun = std::ranges::find_if(ir_module->functions, [=](auto ir_x) {
                        return ir_x->fullname == ir_function->fullname;
                    });
                    // 声明过了, 就不在声明了
                    if (iter_fun != ir_module->functions.end()) {
                        ir_decl_function = *iter_fun;
                    } else {
                        ir_decl_function = ir::Function::Create(ir_function->function_type);
                        ir_decl_function->fullname = ir_function->fullname;
                        ir_decl_function->name = ir_function->name;
                        ir_decl_function->parent = ir_module;
                        ir_module->functions.push_front(ir_decl_function);
                    }

                    ir_instruction->SetOperand(i, ir_decl_function);
                }
            }
        }
    });

    return true;
}

inline void ConvertForMultiDimToFor1Dim(std::shared_ptr<ir::Module> ir_module) {
    auto ir_fors = utility::GetAll<ir::For>(ir_module);
    for (auto ir_for : ir_fors) {
        auto ir_builder = lowering::IrBuilder::Create();
        ir_builder->symbol_table = ir_module->symbol_table;
        // 只需要对数组循环进行处理
        if (!ir_builder->IsArrayI64Type(ir_for->IndexVariable()->type)) continue;
        auto parent = ir_for->GetParentBlock();
        auto scope = ir_builder->PushBlockRAII(parent);
        ir_builder->inserter_iterator = parent->Find(ir_for);
        auto ir_layout_template_struct = lowering::SymbolGet<lowering::TemplateStruct>(
            ir_builder->GetSymbolByPath(true, {"tensor", "Layout"}));
        PRAJNA_ASSERT(ir_layout_template_struct);
        auto ir_array_first = ir_for->First();
        auto ir_array_last = ir_for->Last();
        auto ir_array_type = ir_array_last->type;
        auto ir_array_template_arguments =
            std::any_cast<std::list<lowering::Symbol>>(ir_array_type->template_arguments_any);
        auto ir_rank = lowering::SymbolGet<ir::ConstantInt>(ir_array_template_arguments.back());
        std::list<lowering::Symbol> template_arguments = {ir_rank};
        auto ir_layout_type = ir_layout_template_struct->Instantiate(template_arguments, ir_module);
        // TODO: should use static_function_dict
        auto ir_layout =
            ir_builder->Create<ir::Call>(ir_builder->GetStaticFunction(ir_layout_type, "Create"),
                                         std::list<std::shared_ptr<ir::Value>>{ir_for->Last()});
        auto ir_linear_first = ir_builder->GetConstant<int64_t>(0);

        // auto ir_array_one = lowering::SymbolGet<lowering::Template
        // ir_builder->GetSymbolByPath(true, {"_array", "Array"}); auto ir_arra

        auto ir_array_one_template = lowering::SymbolGet<lowering::Template>(
            ir_builder->GetSymbolByPath(true, {"_array", "__ArrayOne"}));
        PRAJNA_ASSERT(ir_array_one_template);
        auto ir_array_one_fun = lowering::SymbolGet<ir::Value>(
            ir_array_one_template->Instantiate({ir_array_template_arguments.back()}, ir_module));

        // TODO: 可能涉及模板特化, 后面再做处理
        auto ir_array_one = ir_builder->Call(ir_array_one_fun);
        auto ir_array_range = ir_builder->CallBinaryOperator(
            ir_builder->CallBinaryOperator(ir_array_last, "-", ir_array_first), "-", ir_array_one);
        auto ir_linear_last = ir_builder->CallBinaryOperator(
            ir_builder->CallMemberFunction(ir_layout, "ArrayIndexToLinearIndex", {ir_array_range}),
            "+", ir_builder->GetConstant<int64_t>(1));

        ir_for->First(ir_linear_first);
        ir_for->Last(ir_linear_last);
        auto ir_array_index = ir_for->IndexVariable();
        auto ir_linear_index = ir_builder->Create<ir::LocalVariable>(ir::i64);
        ir_for->IndexVariable(ir_linear_index);

        auto ir_array_first_variable = ir_builder->VariableLikedNormalize(ir_array_first);
        auto ir_layout_variable = ir_builder->VariableLikedNormalize(ir_layout);

        {
            auto scope = ir_builder->PushBlockRAII(ir_for->LoopBlock());
            ir_builder->inserter_iterator = ir_for->LoopBlock()->begin();

            utility::RemoveFromParent(ir_array_index);
            ir_builder->Insert(ir_array_index);
            ir_builder->Create<ir::WriteVariableLiked>(
                ir_builder->CallBinaryOperator(
                    ir_builder->CallMemberFunction(ir_layout_variable, "LinearIndexToArrayIndex",
                                                   {ir_linear_index}),
                    "+", ir_array_first_variable),
                ir_array_index);
        }
    }
}

inline bool WrapIntrinsicFunction(std::shared_ptr<ir::Module> ir_module) {
    bool re = false;
    for (auto ir_function : ir_module->functions) {
        if (ir_function->annotation_dict.count("intrinsic")) {
            re = true;
            PRAJNA_ASSERT(!ir_function->annotation_dict["intrinsic"].empty());
            auto intrinsic_function_name = ir_function->annotation_dict["intrinsic"].front();
            auto ir_decl_function = ir::Function::Create(ir_function->function_type);
            ir_decl_function->fullname = intrinsic_function_name;
            ir_decl_function->parent = ir_module;
            ir_module->functions.push_front(ir_decl_function);

            ir_function->annotation_dict["inline"];

            PRAJNA_ASSERT(ir_function->blocks.empty());
            auto ir_builder = lowering::IrBuilder::Create();
            ir_builder->CreateTopBlockForFunction(ir_function);
            auto ir_call = ir_builder->Create<ir::Call>(
                ir_decl_function,
                To<std::list<std::shared_ptr<ir::Value>>>(ir_function->parameters));
            if (!Is<ir::VoidType>(ir_decl_function->function_type->return_type)) {
                ir_builder->Create<ir::Return>(ir_call);
            } else {
                ir_builder->ReturnVoid();
            }

            ir_function->annotation_dict.erase("intrinsic");
        }
    }

    return re;
}

inline bool ConvertClosure(std::shared_ptr<ir::Module> ir_module) {
    for (auto iter_function = ir_module->functions.rbegin();
         iter_function != ir_module->functions.rend(); ++iter_function) {
        auto ir_function = *iter_function;
        if (!ir_function->closure) continue;

        auto ir_external_values = utility::CaptureExternalValueInClosure(ir_function);
        std::map<std::shared_ptr<ir::Value>, std::shared_ptr<ir::Field>> ir_value_field_map;

        // ir_this 存在两个, 之前的不太好获取, 故有搞了一个
        auto ir_this = ir::DeferencePointer::Create(ir_function->parameters.front());
        ir_function->blocks.front()->PushFront(ir_this);

        Each<ir::Value>(ir_function, [&](std::shared_ptr<ir::Value> ir_value) {
            if (auto ir_instruction = Cast<ir::Instruction>(ir_value)) {
                for (int64_t i = 0; i < ir_instruction->OperandSize(); ++i) {
                    auto ir_operand = ir_instruction->GetOperand(i);
                    if (ir::IsGlobal(ir_operand)) continue;
                    if (ir_operand->GetParentFunction() != ir_function) {
                        if (!ir_value_field_map[ir_operand]) {
                            ir_value_field_map[ir_operand] =
                                ir::Field::Create(ir_value->name, ir_operand->type);
                        }
                        auto ir_field = ir_value_field_map[ir_operand];
                        auto ir_access_field =
                            ir::AccessField::Create(ir_this, ir_value_field_map[ir_operand]);
                        auto parent = ir_instruction->GetParentBlock();
                        parent->Insert(ir_instruction->GetBlockIterator(), ir_access_field);
                        ir_access_field->parent = parent;
                        ir_instruction->SetOperand(i, ir_access_field);
                    }
                }
            }
        });

        auto ir_closure_struct_type = Cast<ir::StructType>(ir_this->type);
        PRAJNA_ASSERT(ir_closure_struct_type);
        std::list<std::shared_ptr<ir::Field>> ir_field_list;
        for (auto [ir_value, ir_field] : ir_value_field_map) {
            ir_field_list.push_back(ir_field);
        }
        ir_closure_struct_type->fields = ir_field_list;
        ir_closure_struct_type->Update();

        auto ir_closure = ir_function->closure;
        PRAJNA_ASSERT(ir_closure);
        auto ir_builder = lowering::IrBuilder::Create();
        auto scope = ir_builder->PushBlockRAII(ir_closure->GetParentBlock());
        ir_builder->inserter_iterator = std::next(ir_closure->GetBlockIterator());
        for (auto [ir_value, ir_field] : ir_value_field_map) {
            auto ir_access_filed =
                ir_builder->Create<ir::AccessField>(ir_closure, ir_value_field_map[ir_value]);
            ir_builder->Create<ir::WriteVariableLiked>(ir_value, ir_access_filed);
        }
    }

    return false;
}

inline bool ExternCFunction(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        if (ir_function->annotation_dict.count("extern")) {
            // @extern的全名不加前缀
            ir_function->fullname = ir_function->name;
            // auto ir_decl_function = ir::Function::Create(ir_function->function_type);
            // ir_decl_function->fullname = ir_function->name;
            // ir_decl_function->parent_module = ir_module;
            // ir_module->functions.push_front(ir_decl_function);

            // ir_function->annotation_dict["inline"];

            // PRAJNA_ASSERT(ir_function->blocks.empty());
            // auto ir_builder = lowering::IrBuilder::Create();
            // ir_builder->CreateTopBlockForFunction(ir_function);
            // auto ir_call = ir_builder->Create<ir::Call>(ir_decl_function,
            // ir_function->parameters); if
            // (!Is<ir::VoidType>(ir_decl_function->function_type->return_type)) {
            //     ir_builder->Create<ir::Return>(ir_call);
            // }

            ir_function->annotation_dict.erase("extern");
        }
    }

    return false;
}

inline void TopologicalSortFunctionVisit(
    std::shared_ptr<ir::Function> ir_function,
    std::list<std::shared_ptr<ir::Function>> &ir_function_list,
    std::set<std::shared_ptr<ir::Function>> &ir_gray_function_set) {
    // 标记要访问的函数
    ir_gray_function_set.insert(ir_function);
    auto ir_instructions = utility::GetAll<ir::Instruction>(ir_function);
    for (auto ir_instruction : ir_instructions) {
        for (int64_t i = 0; i < ir_instruction->OperandSize(); ++i) {
            auto ir_operand = ir_instruction->GetOperand(i);
            if (auto ir_tmp_function = Cast<ir::Function>(ir_operand)) {
                // 值排序同一个module里的函数
                if (ir_tmp_function->parent.lock() == ir_function->parent.lock()) {
                    // 没访问的进行深度搜索
                    if (ir_gray_function_set.count(ir_tmp_function)) continue;

                    TopologicalSortFunctionVisit(ir_tmp_function, ir_function_list,
                                                 ir_gray_function_set);
                }
            }
        }
    }

    // 完成搜索后插入函数
    ir_function_list.push_back(ir_function);
}

inline void TopologicalSortFunction(std::shared_ptr<ir::Module> ir_module) {
    std::list<std::shared_ptr<ir::Function>> ir_function_list;
    std::set<std::shared_ptr<ir::Function>> ir_gray_function_set;
    for (auto ir_function : ir_module->functions) {
        if (ir_gray_function_set.count(ir_function)) continue;

        TopologicalSortFunctionVisit(ir_function, ir_function_list, ir_gray_function_set);
    }

    ir_module->functions.remove_if(
        [=](auto ir_function) { return std::ranges::count(ir_function_list, ir_function); });
    ir_module->functions.insert(ir_module->functions.end(), ir_function_list.begin(),
                                ir_function_list.end());
}

inline void TopAlloca(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        if (ir_function->blocks.empty()) continue;

        auto ir_top_block = ir_function->blocks.front();
        auto ir_allocas = utility::GetAll<ir::Alloca>(ir_function);
        for (auto ir_alloca : ir_allocas) {
            auto parent = ir_alloca->GetParentBlock();
            if ((parent && parent != ir_top_block) && Is<ir::ConstantInt>(ir_alloca->Length())) {
                utility::RemoveFromParent(ir_alloca);
                ir_top_block->PushFront(ir_alloca);
                utility::RemoveFromParent(ir_alloca->Length());
                ir_top_block->PushFront(ir_alloca->Length());
            }
        }
    }
}

inline void ConvertLLVMIntrinsicToNVVMLibdevice(std::shared_ptr<ir::Module> ir_module) {
    auto ir_nvptx_module = ir_module->modules[ir::Target::nvptx];
    if (!ir_nvptx_module) return;

    std::list<ir::Function> ir_llvm_intrinsics;
    std::map<std::string, std::string> name_map = {{"llvm.sin.f32", "__nv_sinf"}};
    for (auto ir_function : ir_nvptx_module->functions) {
        if (name_map.count(ir_function->fullname)) {
            ir_function->fullname = name_map[ir_function->fullname];
        }
    };
}

// 将 LLVM 内建数学函数名替换为 AMDGPU 后端 libdevice（OCML）库函数名，
// 以便在 .ll 转换为 .hsaco 时链接 libdevice（如 ocml.bc）成功。
//
// 参考 ROCm 提供的 HIP 数学头文件源码（包含 __ocml_* 的映射关系）：
// https://rocm.docs.amd.com/projects/HIP/en/develop/doxygen/html/____clang__hip__math_8h_source.html
//
// Example:
//   llvm.sin.f32  ->  __ocml_sin_f32
//   llvm.asin.f32 ->  __ocml_asin_f32

inline void ConvertLLVMIntrinsicToAmdGPULibdevice(std::shared_ptr<ir::Module> ir_module) {
    auto ir_amdgpu_module = ir_module->modules[ir::Target::amdgpu];
    if (!ir_amdgpu_module) return;

    std::list<ir::Function> ir_llvm_intrinsics;
    // 替换 LLVM intrinsics 为 OCML 函数
    std::map<std::string, std::string> name_map = {{"llvm.sin.f32", "__ocml_sin_f32"},
                                                   {"llvm.cos.f32", "__ocml_cos_f32"},
                                                   {"llvm.asin.f32", "__ocml_asin_f32"},
                                                   {"llvm.acos.f32", "__ocml_acos_f32"}

    };
    for (auto ir_function : ir_amdgpu_module->functions) {
        if (name_map.count(ir_function->fullname)) {
            ir_function->fullname = name_map[ir_function->fullname];
        }
    };
}

inline void ConvertSharedMemoryLocalVariableToGlobalAlloca(std::shared_ptr<ir::Module> ir_module) {
    auto ir_shared_variable_list = utility::GetAll<ir::LocalVariable>(ir_module);
    ir_shared_variable_list.remove_if([](auto ir_local_variable) {
        return ir_local_variable->annotation_dict.count("shared") == 0;
    });

    for (auto ir_shared_variable : ir_shared_variable_list) {
        auto ir_global_alloca = ir::GlobalAlloca::Create(ir_shared_variable->type);
        ir_global_alloca->address_space = 3;  // nvptx/amd shared memory
        ir_global_alloca->name = ir_shared_variable->name;
        if (ir_module->modules[ir::Target::nvptx]) {
            ir_global_alloca->fullname = MangleNvvmName(ir_shared_variable->fullname);
        } else if (ir_module->modules[ir::Target::amdgpu]) {
            ir_global_alloca->fullname = MangleHipName(ir_shared_variable->fullname);
        }

        ir_global_alloca->is_external = false;
        ir_module->AddGlobalAlloca(ir_global_alloca);

        auto ir_builder = lowering::IrBuilder::Create();
        auto parent = ir_shared_variable->GetParentBlock();
        auto scope = ir_builder->PushBlockRAII(parent);
        // 在最开始插入就行, 留意AddressCast是不是统一转换一次就行了
        ir_builder->inserter_iterator = parent->begin();
        auto ir_address_cast =
            ir_builder->Create<ir::CastInstruction>(ir::CastInstruction::Operation::AddrSpaceCast,
                                                    ir_global_alloca, ir_global_alloca->type);
        auto ir_deference_pointer = ir_builder->Create<ir::DeferencePointer>(ir_address_cast);

        for (auto [ir_instruction, op_idx] :
             Clone(ir_shared_variable->instruction_with_index_list)) {
            Lock(ir_instruction)->SetOperand(op_idx, ir_deference_pointer);
        }

        utility::RemoveFromParent(ir_shared_variable);
        ir_shared_variable->Finalize();
    }
}

inline bool InsertLocationForAssert(std::shared_ptr<ir::Module> ir_module) {
    auto ir_calls = utility::GetAll<ir::Call>(ir_module);
    for (auto ir_call : ir_calls) {
        if (auto ir_callee = Cast<ir::Function>(ir_call->Function())) {
            if (ir_callee->fullname == "::test::Assert") {
                auto iter = ir_call->GetBlockIterator();
                auto ir_builder =
                    lowering::IrBuilder::Create(ir_module->symbol_table, ir_module, nullptr);
                auto scope = ir_builder->PushBlockRAII(ir_call->GetParentBlock());
                ir_builder->inserter_iterator = iter;
                auto position = ir_call->source_location.first_position;
                auto filename = ir_builder->GetString(position.file);
                auto line = ir_builder->GetString(std::to_string(position.line));
                auto ir_print_location = lowering::SymbolGet<ir::Value>(
                    ir_builder->GetSymbolByPath(false, {"test", "AssertWithPosition"}));
                auto ir_condition = ir_call->Argument(0);
                ir_builder->Create<ir::Call>(
                    ir_print_location,
                    std::list<std::shared_ptr<ir::Value>>{ir_condition, filename, line});
                utility::RemoveFromParent(ir_call);
                ir_call->Finalize();
                continue;
            }
            if (ir_callee->fullname == "::debug::Assert") {
                auto iter = ir_call->GetBlockIterator();
                auto ir_builder =
                    lowering::IrBuilder::Create(ir_module->symbol_table, ir_module, nullptr);
                auto scope = ir_builder->PushBlockRAII(ir_call->GetParentBlock());
                ir_builder->inserter_iterator = iter;
                auto position = ir_call->source_location.first_position;
                auto filename = ir_builder->GetString(position.file);
                auto line = ir_builder->GetString(std::to_string(position.line));
                auto ir_print_location = lowering::SymbolGet<ir::Value>(
                    ir_builder->GetSymbolByPath(false, {"debug", "AssertWithPosition"}));
                auto ir_condition = ir_call->Argument(0);
                ir_builder->Create<ir::Call>(
                    ir_print_location,
                    std::list<std::shared_ptr<ir::Value>>{ir_condition, filename, line});
                utility::RemoveFromParent(ir_call);
                ir_call->Finalize();
                continue;
            }
        }
    }

    return false;
}

inline std::shared_ptr<ir::Module> transform(std::shared_ptr<ir::Module> ir_module) {
    PRAJNA_ASSERT(ir::Verify(ir_module));
    RecursiveTransformModule(ir_module, ConvertClosure);
    RecursiveTransformModule(ir_module, WrapIntrinsicFunction);
    RecursiveTransformModule(ir_module, ExternCFunction);
    RecursiveTransformModule(ir_module, InsertLocationForAssert);
    ConvertForMultiDimToFor1Dim(ir_module);
    RecursiveTransformModule(ir_module, ConvertPropertyToFunctionCall);
    InsertReferenceCount(ir_module);
    TopologicalSortFunction(ir_module);
    // ExtractGpuFor(ir_module);
    ConvertKernelFunctionOperandToAddress(ir_module);
    ConvertKernelFunctionCallToKernelLaunch(ir_module);
    RecursiveTransformModule(ir_module, InlineFunction);
    RecursiveTransformModule(ir_module, FlatternBlock);
    RemoveValuesAfterReturn(ir_module);
    RecursiveTransformModule(ir_module, ConvertPropertyToFunctionCall);
    ConvertGlobalVariableToPointer(ir_module);
    ConvertSharedMemoryLocalVariableToGlobalAlloca(ir_module);
    // ssa
    bool changed = true;
    while (changed) {
        changed = RecursiveTransformModule(ir_module, InsertValueToBlock);
        changed = RecursiveTransformModule(ir_module, ConvertVariableToDeferencePointer) || changed;
        changed =
            RecursiveTransformModule(ir_module, ConvertAccessFieldToGetStructElementPointer) ||
            changed;
        changed = RecursiveTransformModule(ir_module, ConvertIndexArrayToGetArrayElementPointer) ||
                  changed;
        changed =
            RecursiveTransformModule(ir_module, ConvertIndexPointerToGetPointerElementPointer) ||
            changed;
        changed = RecursiveTransformModule(ir_module, ConvertGetAddressOfVaraibleLikedToPointer) ||
                  changed;
    }
    // 需要全部转为Deference才能进行, 因为上面的转换是围绕其进行的
    RecursiveTransformModule(ir_module, ConvertDeferencePointerToStoreAndLoadPointer);
    //
    SperateModule(ir_module);
    CloneExternalDeviceValue(ir_module, ir::Target::nvptx);
    CloneExternalDeviceValue(ir_module, ir::Target::amdgpu);

    DefineKernelFunctionAddress(ir_module, ir::Target::nvptx);
    DefineKernelFunctionAddress(ir_module, ir::Target::amdgpu);

    ConvertGlobalVariableToPointer(ir_module);
    RecursiveTransformModule(ir_module, DeclareExternalFunction);
    TopAlloca(ir_module);

    ConvertLLVMIntrinsicToNVVMLibdevice(ir_module);
    ConvertLLVMIntrinsicToAmdGPULibdevice(ir_module);

    PRAJNA_ASSERT(RecursiveTransformModule(ir_module, ir::Verify));

    return ir_module;
}

}  // namespace prajna::transform
