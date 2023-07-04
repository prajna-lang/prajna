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
#include "prajna/transform/verify.hpp"
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
        if (not ir_sub_module) continue;

        fun(ir_sub_module);
    }

    return re;
}

std::shared_ptr<ir::Module> ConvertVariableToPointer(std::shared_ptr<ir::Module> ir_module);

std::shared_ptr<ir::Module> MakeCompatiableWithLlvm(std::shared_ptr<ir::Module> ir_module);

std::shared_ptr<ir::Module> SperateModule(std::shared_ptr<ir::Module> ir_module);

inline bool ConvertPropertyToFunctionCall(std::shared_ptr<ir::Module> ir_module) {
    auto ir_access_properties = utility::GetValuesInModule<ir::AccessProperty>(ir_module);
    for (auto ir_access_property : ir_access_properties) {
        auto ir_block = ir_access_property->parent_block;
        auto ir_builder = lowering::IrBuilder::Create();
        ir_builder->PushBlock(ir_block);
        ir_builder->inserter_iterator = std::find(RANGE(ir_block->values), ir_access_property);

        bool unused = ir_access_property->instruction_with_index_list.empty();
        for (auto instruction_with_index : Clone(ir_access_property->instruction_with_index_list)) {
            auto ir_inst = instruction_with_index.instruction;
            size_t op_idx = instruction_with_index.operand_index;

            PRAJNA_ASSERT(not Is<ir::GetAddressOfVariableLiked>(ir_inst));

            if (Is<ir::WriteProperty>(ir_inst) && op_idx == 1) {
                auto ir_write_property = cast<ir::WriteProperty>(ir_inst);
                auto ir_arguments = ir_access_property->arguments();
                ir_arguments.insert(ir_arguments.begin(), ir_access_property->ThisPointer());
                ir_arguments.push_back(ir_write_property->value());
                auto ir_setter_call = ir_builder->Create<ir::Call>(
                    ir_access_property->property->set_function, ir_arguments);
                utility::RemoveFromParent(ir_write_property);
                ir_write_property->Finalize();
            } else {
                auto ir_arguments = ir_access_property->arguments();
                ir_arguments.insert(ir_arguments.begin(), ir_access_property->ThisPointer());
                auto ir_getter_call = ir_builder->Create<ir::Call>(
                    ir_access_property->property->get_function, ir_arguments);
                ir_inst->operand(op_idx, ir_getter_call);
            }
        }

        if (unused) {
            auto ir_arguments = ir_access_property->arguments();
            ir_arguments.insert(ir_arguments.begin(), ir_access_property->ThisPointer());
            auto ir_getter_call = ir_builder->Create<ir::Call>(
                ir_access_property->property->get_function, ir_arguments);
        }

        utility::RemoveFromParent(ir_access_property);
        ir_access_property->Finalize();
    }

    return !ir_access_properties.empty();
}

inline void ConvertKernelFunctionCallToKernelLaunch(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_kernel_function_calls =
            utility::GetValuesInFunction<ir::KernelFunctionCall>(ir_function);
        for (auto ir_kernel_function_call : ir_kernel_function_calls) {
            auto ir_kernel_function = ir_kernel_function_call->Function();
            auto ir_grid_shape = ir_kernel_function_call->GridShape();
            auto ir_block_shape = ir_kernel_function_call->BlockShape();
            // auto ir_arguments = ir_kernel_function_call->parameters();
            auto ir_block = ir_kernel_function_call->parent_block;

            auto ir_builder =
                lowering::IrBuilder::Create(ir_module->symbol_table, ir_module, nullptr);
            ir_builder->PushBlock(ir_block);
            ir_builder->inserter_iterator =
                std::find(RANGE(ir_block->values), ir_kernel_function_call);

            // 构建::cuda::launchKernel的逻辑
            auto ir_kernel_arguments_address_array_i8ptr = ir_builder->Create<ir::LocalVariable>(
                ir::ArrayType::Create(ir::PointerType::Create(ir::IntType::Create(8, true)),
                                      ir_kernel_function_call->ArgumentSize()));
            for (size_t i = 0; i < ir_kernel_function_call->ArgumentSize(); ++i) {
                auto ir_argument = ir_kernel_function_call->Argument(i);
                auto ir_kernel_argument_address_i8ptr = ir_builder->Create<ir::BitCast>(
                    ir_builder->Create<ir::GetAddressOfVariableLiked>(
                        ir_builder->VariableLikedNormalize(ir_argument)),
                    ir::PointerType::Create(ir::IntType::Create(8, true)));
                auto ir_array_index = ir_builder->Create<ir::IndexArray>(
                    ir_kernel_arguments_address_array_i8ptr, ir_builder->GetIndexConstant(i));
                auto ir_array_index_write = ir_builder->Create<ir::WriteVariableLiked>(
                    ir_kernel_argument_address_i8ptr, ir_array_index);
            }

            auto ir_launch_function = lowering::SymbolGet<ir::Value>(
                lowering::SymbolGet<lowering::SymbolTable>(
                    ir_module->symbol_table->RootSymbolTable()->Get("gpu"))
                    ->Get("LaunchKernel"));
            PRAJNA_ASSERT(ir_launch_function);
            std::list<std::shared_ptr<ir::Value>> ir_arguments;
            ir_arguments.push_back(ir_builder->Create<ir::BitCast>(
                ir_kernel_function, ir::PointerType::Create(ir::IntType::Create(8, true))));
            ir_arguments.push_back(ir_grid_shape);
            ir_arguments.push_back(ir_block_shape);
            auto ir_array_index0 = ir_builder->Create<ir::IndexArray>(
                ir_kernel_arguments_address_array_i8ptr, ir_builder->GetIndexConstant(0));
            auto ir_array_address = ir_builder->Create<ir::BitCast>(
                ir_builder->Create<ir::GetAddressOfVariableLiked>(ir_array_index0),
                ir::PointerType::Create(ir::PointerType::Create(ir::IntType::Create(8, true))));
            ir_arguments.push_back(ir_array_address);
            auto ir_kernel_call = ir_builder->Create<ir::Call>(ir_launch_function, ir_arguments);
            auto ir_zero = ir_builder->GetIndexConstant(0);
            auto ir_condition = ir_builder->CallBinaryOperator(ir_kernel_call, "!=", ir_zero);
            auto ir_if =
                ir_builder->Create<ir::If>(ir_condition, ir::Block::Create(), ir::Block::Create());

            ir_builder->PushBlock(ir_if->TrueBlock());
            auto ir_str = ir_builder->GetString("Failed launch kernel: ret: ");
            ir_builder->CallMemberFunction(ir_str, "Print", {});
            ir_builder->CallMemberFunction(ir_kernel_call, "PrintLine", {});
            ir_builder->PopBlock();

            utility::RemoveFromParent(ir_kernel_function_call);
            ir_kernel_function_call->Finalize();
        }
    }
}

inline void ConvertKernelFunctionOperandToAddress(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_instructions = utility::GetValuesInFunction<ir::Instruction>(ir_function);

        for (auto ir_instruction : ir_instructions) {
            for (size_t i = 0; i < ir_instruction->OperandSize(); ++i) {
                if (auto ir_function = cast<ir::Function>(ir_instruction->operand(i))) {
                    if (ir_function->annotation_dict.count("kernel")) {
                        auto global_variable_fullname = GetKernelFunctionAddressName(ir_function);

                        std::shared_ptr<ir::GlobalVariable> ir_global_variable = nullptr;
                        auto iter_global_variable =
                            std::find_if(RANGE(ir_module->global_variables),
                                         [=](std::shared_ptr<ir::GlobalVariable> x) {
                                             return x->fullname == global_variable_fullname;
                                         });
                        if (iter_global_variable != ir_module->global_variables.end()) {
                            ir_global_variable = *iter_global_variable;
                        } else {
                            ir_global_variable = ir::GlobalVariable::Create(ir_function->type);
                            ir_global_variable->name = global_variable_fullname;
                            ir_global_variable->fullname = ir_global_variable->name;
                            ir_global_variable->parent_module = ir_module;
                            // 如果不是同一个module的, 则为external, 目前所有的nvptx
                            // IR都会迁移的使用的Module里去
                            // ir_global_variable->is_external =
                            //     ir_function->parent_module != ir_module;
                            ir_module->global_variables.push_back(ir_global_variable);
                        }

                        ir_instruction->operand(i, ir_global_variable);
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
        ir_global_alloca->parent_module = ir_module;
        // ir_global_variable->is_external默认为false
        ir_global_alloca->is_external = ir_global_variable->is_external;
        ir_module->global_allocas.push_back(ir_global_alloca);
    }

    ir_module->global_variables.clear();

    for (auto ir_function : ir_module->functions) {
        auto ir_instructions = utility::GetValuesInFunction<ir::Instruction>(ir_function);
        for (auto ir_instruction : ir_instructions) {
            for (size_t i = 0; i < ir_instruction->OperandSize(); ++i) {
                // @note 全局变量目前遵循如果使用其他module的则自身为external的原则
                if (auto ir_global_variable =
                        cast<ir::GlobalVariable>(ir_instruction->operand(i))) {
                    auto iter_global_alloca = std::find_if(
                        RANGE(ir_module->global_allocas), [=](std::shared_ptr<ir::GlobalAlloca> x) {
                            return x->fullname == ir_global_variable->fullname;
                        });
                    std::shared_ptr<ir::GlobalAlloca> ir_global_alloca = nullptr;
                    if (iter_global_alloca != ir_module->global_allocas.end()) {
                        ir_global_alloca = *iter_global_alloca;
                    } else {
                        ir_global_alloca = ir::GlobalAlloca::Create(ir_global_variable->type);
                        ir_global_alloca->name = ir_global_variable->name;
                        ir_global_alloca->fullname = ir_global_variable->fullname;
                        ir_global_alloca->parent_module = ir_module;
                        ir_global_alloca->is_external = true;
                        ir_module->global_allocas.push_back(ir_global_alloca);
                    }

                    auto ir_deference_pointer = ir::DeferencePointer::Create(ir_global_alloca);
                    auto ir_block = ir_instruction->parent_block;
                    auto iter =
                        std::find(ir_block->values.begin(), ir_block->values.end(), ir_instruction);
                    ir_block->insert(iter, ir_deference_pointer);
                    ir_instruction->operand(i, ir_deference_pointer);
                }
            }
        }
    }
}

inline void CloneExternalNvptxValue(std::shared_ptr<ir::Module> ir_module) {
    if (ir_module->modules.count(ir::Target::nvptx) == 0) {
        return;
    }

    auto ir_nvptx_module = ir_module->modules[ir::Target::nvptx];

    ir_module->global_allocas.remove_if([=](auto ir_global_alloca) -> bool {
        if (ir_global_alloca->address_space == 3) {
            ir_nvptx_module->global_allocas.push_back(ir_global_alloca);
            ir_global_alloca->parent_module = ir_nvptx_module;
            return true;
        } else {
            return false;
        }
    });

    std::list<std::shared_ptr<ir::Function>> ir_kernel_function_list;
    std::copy_if(RANGE(ir_nvptx_module->functions), std::back_inserter(ir_kernel_function_list),
                 [](std::shared_ptr<ir::Function> ir_function) {
                     return ir_function->annotation_dict.count("kernel");
                 });

    auto function_cloner = ir::FunctionCloner::Create(ir_nvptx_module);
    for (auto ir_kernel_function : ir_kernel_function_list) {
        // 会把生成的函数直接插入到module里
        ir_kernel_function->Clone(function_cloner);
        // 移除原来的核函数
        ir_nvptx_module->functions.remove(ir_kernel_function);
    }
}

inline void DefineKernelFunctionAddress(std::shared_ptr<ir::Module> ir_module) {
    if (ir_module->modules.count(ir::Target::nvptx) == 0) {
    }

    auto ir_nvptx_module = ir_module->modules[ir::Target::nvptx];
    for (auto ir_function : ir_nvptx_module->functions) {
        if (ir_function->annotation_dict.count("kernel")) {
            auto global_variable_fullname = GetKernelFunctionAddressName(ir_function);
            auto iter_global_variable = std::find_if(
                RANGE(ir_module->global_variables), [=](std::shared_ptr<ir::GlobalVariable> x) {
                    return x->fullname == global_variable_fullname;
                });
            if (iter_global_variable == ir_module->global_variables.end()) {
                auto ir_global_variable = ir::GlobalVariable::Create(ir_function->type);
                ir_global_variable->name = global_variable_fullname;
                ir_global_variable->fullname = ir_global_variable->name;
                ir_global_variable->parent_module = ir_module;
                ir_module->global_variables.push_back(ir_global_variable);
            }
        }
    }
}

// @brief 移除return后面的指令. 若不移除, 会存在未知错误
inline void RemoveValuesAfterReturn(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        for (auto ir_block : ir_function->blocks) {
            auto iter_return = std::find_if(RANGE(ir_block->values), [](auto x) {
                return Is<ir::Return>(x) || Is<ir::JumpBranch>(x) || Is<ir::ConditionBranch>(x);
            });
            if (iter_return != ir_block->values.end()) {
                ir_block->values.erase(std::next(iter_return), ir_block->values.end());
            }
        }
    }
}

inline void DeclareExternalFunction(std::shared_ptr<ir::Module> ir_module) {
    utility::Each<ir::Instruction>(ir_module, [=](std::shared_ptr<ir::Instruction> ir_instruction) {
        for (size_t i = 0; i < ir_instruction->OperandSize(); ++i) {
            auto ir_operand = ir_instruction->operand(i);

            if (auto ir_function = cast<ir::Function>(ir_operand)) {
                if (ir_function->parent_module != ir_module) {
                    std::shared_ptr<ir::Function> ir_decl_function = nullptr;
                    auto iter_fun = std::find_if(RANGE(ir_module->functions), [=](auto ir_x) {
                        return ir_x->fullname == ir_function->fullname;
                    });
                    // 声明过了, 就不在声明了
                    if (iter_fun != ir_module->functions.end()) {
                        ir_decl_function = *iter_fun;
                    } else {
                        ir_decl_function = ir::Function::Create(ir_function->function_type);
                        ir_decl_function->fullname = ir_function->fullname;
                        ir_decl_function->name = ir_function->name;
                        ir_decl_function->parent_module = ir_module;
                        ir_module->functions.push_front(ir_decl_function);
                    }

                    auto instruction_with_index_list_copy =
                        ir_function->instruction_with_index_list;
                    for (auto [ir_instruction, op_idx] : instruction_with_index_list_copy) {
                        if (ir_instruction->GetParentFunction()->parent_module == ir_module) {
                            ir_instruction->operand(op_idx, ir_decl_function);
                        }
                    }
                }
            }
        }
    });
}

inline void ConvertForMultiDimToFor1Dim(std::shared_ptr<ir::Module> ir_module) {
    auto ir_fors = utility::GetValuesInModule<ir::For>(ir_module);
    for (auto ir_for : ir_fors) {
        auto ir_builder = lowering::IrBuilder::Create();
        ir_builder->symbol_table = ir_module->symbol_table;
        // 只需要对数组循环进行处理
        if (not ir_builder->IsArrayIndexType(ir_for->index()->type)) continue;

        ir_builder->PushBlock(ir_for->parent_block);
        ir_builder->inserter_iterator = ir_for->parent_block->find(ir_for);
        auto ir_layout_template_struct = lowering::SymbolGet<lowering::TemplateStruct>(
            ir_builder->GetSymbolByPath(true, {"tensor", "Layout"}));
        PRAJNA_ASSERT(ir_layout_template_struct);
        auto ir_array_first = ir_for->first();
        auto ir_array_last = ir_for->last();
        auto ir_array_type = ir_array_last->type;
        auto ir_array_template_arguments =
            std::any_cast<std::list<lowering::Symbol>>(ir_array_type->template_arguments_any);
        auto ir_rank = lowering::SymbolGet<ir::ConstantInt>(ir_array_template_arguments.back());
        std::list<lowering::Symbol> template_arguments = {ir_rank};
        auto ir_layout_type = ir_layout_template_struct->Instantiate(template_arguments, ir_module);
        auto ir_layout =
            ir_builder->Create<ir::Call>(ir_builder->GetImplementFunction(ir_layout_type, "Create"),
                                         std::list<std::shared_ptr<ir::Value>>{ir_for->last()});
        auto ir_linear_first = ir_builder->GetIndexConstant(0);

        auto ir_array_one =
            ir_builder->Create<ir::Call>(ir_builder->GetImplementFunction(ir_array_type, "One"));
        auto ir_array_range = ir_builder->CallBinaryOperator(
            ir_builder->CallBinaryOperator(ir_array_last, "-", ir_array_first), "-", ir_array_one);
        auto ir_linear_last = ir_builder->CallBinaryOperator(
            ir_builder->CallMemberFunction(ir_layout, "ArrayIndexToLinearIndex", {ir_array_range}),
            "+", ir_builder->GetIndexConstant(1));

        ir_for->first(ir_linear_first);
        ir_for->last(ir_linear_last);
        auto ir_array_index = ir_for->index();
        auto ir_linear_index = ir_builder->Create<ir::LocalVariable>(ir_builder->GetIndexType());
        ir_for->index(ir_linear_index);

        auto ir_array_first_variable = ir_builder->VariableLikedNormalize(ir_array_first);
        auto ir_layout_variable = ir_builder->VariableLikedNormalize(ir_layout);
        ir_builder->PushBlock(ir_for->LoopBlock());
        ir_builder->inserter_iterator = ir_for->LoopBlock()->values.begin();
        utility::RemoveFromParent(ir_array_index);
        ir_builder->insert(ir_array_index);
        ir_builder->Create<ir::WriteVariableLiked>(
            ir_builder->CallBinaryOperator(
                ir_builder->CallMemberFunction(ir_layout_variable, "LinearIndexToArrayIndex",
                                               {ir_linear_index}),
                "+", ir_array_first_variable),
            ir_array_index);
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
            ir_decl_function->parent_module = ir_module;
            ir_module->functions.push_front(ir_decl_function);

            ir_function->annotation_dict["inline"];

            PRAJNA_ASSERT(ir_function->blocks.empty());
            auto ir_builder = lowering::IrBuilder::Create();
            ir_builder->CreateTopBlockForFunction(ir_function);
            auto ir_call = ir_builder->Create<ir::Call>(ir_decl_function, ir_function->parameters);
            if (!Is<ir::VoidType>(ir_decl_function->function_type->return_type)) {
                ir_builder->Create<ir::Return>(ir_call);
            }

            ir_function->annotation_dict.erase("intrinsic");
        }
    }

    return re;
}

inline void TopologicalSortFunctionVisit(
    std::shared_ptr<ir::Function> ir_function,
    std::list<std::shared_ptr<ir::Function>> &ir_function_list,
    std::set<std::shared_ptr<ir::Function>> &ir_gray_function_set) {
    // 标记要访问的函数
    ir_gray_function_set.insert(ir_function);
    auto ir_instructions = utility::GetValuesInFunction<ir::Instruction>(ir_function);
    for (auto ir_instruction : ir_instructions) {
        for (size_t i = 0; i < ir_instruction->OperandSize(); ++i) {
            auto ir_operand = ir_instruction->operand(i);
            if (auto ir_tmp_function = cast<ir::Function>(ir_operand)) {
                // 值排序同一个module里的函数
                if (ir_tmp_function->parent_module == ir_function->parent_module) {
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
        [=](auto ir_function) { return std::count(RANGE(ir_function_list), ir_function); });
    ir_module->functions.merge(ir_function_list);
}

inline void TopAlloca(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        if (ir_function->blocks.empty()) continue;

        auto ir_top_block = ir_function->blocks.front();
        auto ir_allocas = utility::GetValuesInFunction<ir::Alloca>(ir_function);
        for (auto ir_alloca : ir_allocas) {
            if (ir_alloca->parent_block != ir_top_block) {
                utility::RemoveFromParent(ir_alloca);
                ir_top_block->pushFront(ir_alloca);
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

inline void ConvertSharedMemoryLocalVariableToGlobalAlloca(std::shared_ptr<ir::Module> ir_module) {
    auto ir_shared_variable_list = utility::GetValuesInModule<ir::LocalVariable>(ir_module);
    ir_shared_variable_list.remove_if([](auto ir_local_variable) {
        return ir_local_variable->annotation_dict.count("shared") == 0;
    });

    for (auto ir_shared_variable : ir_shared_variable_list) {
        auto ir_global_alloca = ir::GlobalAlloca::Create(ir_shared_variable->type);
        ir_global_alloca->address_space = 3;  // nvptx shared memory
        ir_global_alloca->name = ir_shared_variable->name;
        ir_global_alloca->fullname = MangleNvvmName(ir_shared_variable->fullname);
        ir_global_alloca->is_external = false;
        ir_global_alloca->parent_module = ir_module;
        ir_module->global_allocas.push_back(ir_global_alloca);

        auto ir_builder = lowering::IrBuilder::Create();
        ir_builder->PushBlock(ir_shared_variable->parent_block);
        // 在最开始插入就行, 留意AddressCast是不是统一转换一次就行了
        ir_builder->inserter_iterator = ir_shared_variable->parent_block->values.begin();
        auto ir_address_cast =
            ir_builder->Create<ir::CastInstruction>(ir::CastInstruction::Operation::AddrSpaceCast,
                                                    ir_global_alloca, ir_global_alloca->type);
        auto ir_deference_pointer = ir_builder->Create<ir::DeferencePointer>(ir_address_cast);

        for (auto [ir_instruction, op_idx] :
             Clone(ir_shared_variable->instruction_with_index_list)) {
            ir_instruction->operand(op_idx, ir_deference_pointer);
        }

        utility::RemoveFromParent(ir_shared_variable);
        ir_shared_variable->Finalize();
    }
}

inline std::shared_ptr<ir::Module> transform(std::shared_ptr<ir::Module> ir_module) {
    RecursiveTransformModule(ir_module, WrapIntrinsicFunction);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    ConvertForMultiDimToFor1Dim(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    RecursiveTransformModule(ir_module, ConvertPropertyToFunctionCall);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    InsertReferenceCount(ir_module);
    TopologicalSortFunction(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    ExtractGpuFor(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    ConvertKernelFunctionOperandToAddress(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    ConvertKernelFunctionCallToKernelLaunch(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    RecursiveTransformModule(ir_module, InlineFunction);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    RecursiveTransformModule(ir_module, FlatternBlock);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    RemoveValuesAfterReturn(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    RecursiveTransformModule(ir_module, ConvertPropertyToFunctionCall);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    ConvertGlobalVariableToPointer(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    PRAJNA_ASSERT(VerifyModule(ir_module));
    ConvertSharedMemoryLocalVariableToGlobalAlloca(ir_module);
    // ssa
    bool changed = true;
    while (changed) {
        changed = RecursiveTransformModule(ir_module, InsertValueToBlock);
        PRAJNA_ASSERT(VerifyModule(ir_module));
        changed = RecursiveTransformModule(ir_module, ConvertVariableToDeferencePointer) || changed;
        PRAJNA_ASSERT(VerifyModule(ir_module));
        changed =
            RecursiveTransformModule(ir_module, ConvertAccessFieldToGetStructElementPointer) ||
            changed;
        PRAJNA_ASSERT(VerifyModule(ir_module));
        changed = RecursiveTransformModule(ir_module, ConvertIndexArrayToGetArrayElementPointer) ||
                  changed;
        PRAJNA_ASSERT(VerifyModule(ir_module));
        changed =
            RecursiveTransformModule(ir_module, ConvertIndexPointerToGetPointerElementPointer) ||
            changed;
        PRAJNA_ASSERT(VerifyModule(ir_module));
        changed = RecursiveTransformModule(ir_module, ConvertGetAddressOfVaraibleLikedToPointer) ||
                  changed;
        PRAJNA_ASSERT(VerifyModule(ir_module));
    }
    // 需要全部转为Deference才能进行, 因为上面的转换是围绕其进行的
    RecursiveTransformModule(ir_module, ConvertDeferencePointerToStoreAndLoadPointer);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    //
    SperateModule(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    CloneExternalNvptxValue(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    DefineKernelFunctionAddress(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    ConvertGlobalVariableToPointer(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    DeclareExternalFunction(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    TopAlloca(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    ConvertLLVMIntrinsicToNVVMLibdevice(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));

    return ir_module;
}

}  // namespace prajna::transform
