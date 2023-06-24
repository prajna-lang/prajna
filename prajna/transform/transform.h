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

std::shared_ptr<ir::Module> convertVariableToPointer(std::shared_ptr<ir::Module> ir_module);

std::shared_ptr<ir::Module> makeCompatiableWithLlvm(std::shared_ptr<ir::Module> ir_module);

std::shared_ptr<ir::Module> sperateModule(std::shared_ptr<ir::Module> ir_module);

inline void convertPropertyToFunctionCall(std::shared_ptr<ir::Module> ir_module) {
    auto ir_access_properties = utility::getValuesInModule<ir::AccessProperty>(ir_module);
    for (auto ir_access_property : ir_access_properties) {
        auto ir_block = ir_access_property->parent_block;
        auto ir_builder = lowering::IrBuilder::create();
        ir_builder->pushBlock(ir_block);
        ir_builder->inserter_iterator = std::find(RANGE(ir_block->values), ir_access_property);

        bool unused = ir_access_property->instruction_with_index_list.empty();
        for (auto instruction_with_index : clone(ir_access_property->instruction_with_index_list)) {
            auto ir_inst = instruction_with_index.instruction;
            size_t op_idx = instruction_with_index.operand_index;

            PRAJNA_ASSERT(not is<ir::GetAddressOfVariableLiked>(ir_inst));

            if (is<ir::WriteProperty>(ir_inst) && op_idx == 1) {
                auto ir_write_property = cast<ir::WriteProperty>(ir_inst);
                auto ir_arguments = ir_access_property->arguments();
                ir_arguments.insert(ir_arguments.begin(), ir_access_property->thisPointer());
                ir_arguments.push_back(ir_write_property->value());
                auto ir_setter_call = ir_builder->create<ir::Call>(
                    ir_access_property->property->set_function, ir_arguments);
                utility::removeFromParent(ir_write_property);
                ir_write_property->finalize();
            } else {
                auto ir_arguments = ir_access_property->arguments();
                ir_arguments.insert(ir_arguments.begin(), ir_access_property->thisPointer());
                auto ir_getter_call = ir_builder->create<ir::Call>(
                    ir_access_property->property->get_function, ir_arguments);
                ir_inst->operand(op_idx, ir_getter_call);
            }
        }

        if (unused) {
            auto ir_arguments = ir_access_property->arguments();
            ir_arguments.insert(ir_arguments.begin(), ir_access_property->thisPointer());
            auto ir_getter_call = ir_builder->create<ir::Call>(
                ir_access_property->property->get_function, ir_arguments);
        }

        utility::removeFromParent(ir_access_property);
        ir_access_property->finalize();
    }

    for (auto [ir_target, ir_sub_module] : ir_module->modules) {
        if (not ir_module) continue;

        convertPropertyToFunctionCall(ir_sub_module);
    }
}

inline void convertKernelFunctionCallToKernelLaunch(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_kernel_function_calls =
            utility::getValuesInFunction<ir::KernelFunctionCall>(ir_function);
        for (auto ir_kernel_function_call : ir_kernel_function_calls) {
            auto ir_kernel_function = ir_kernel_function_call->function();
            auto ir_grid_shape = ir_kernel_function_call->GridShape();
            auto ir_block_shape = ir_kernel_function_call->BlockShape();
            // auto ir_arguments = ir_kernel_function_call->parameters();
            auto ir_block = ir_kernel_function_call->parent_block;

            auto ir_builder = lowering::IrBuilder::create();
            ir_builder->pushBlock(ir_block);
            ir_builder->inserter_iterator =
                std::find(RANGE(ir_block->values), ir_kernel_function_call);

            // 构建::cuda::launchKernel的逻辑
            auto ir_kernel_arguments_address_array_i8ptr = ir_builder->create<ir::LocalVariable>(
                ir::ArrayType::create(ir::PointerType::create(ir::IntType::create(8, true)),
                                      ir_kernel_function_call->argumentSize()));
            for (size_t i = 0; i < ir_kernel_function_call->argumentSize(); ++i) {
                auto ir_argument = ir_kernel_function_call->argument(i);
                auto ir_kernel_argument_address_i8ptr = ir_builder->create<ir::BitCast>(
                    ir_builder->create<ir::GetAddressOfVariableLiked>(
                        ir_builder->variableLikedNormalize(ir_argument)),
                    ir::PointerType::create(ir::IntType::create(8, true)));
                auto ir_array_index = ir_builder->create<ir::IndexArray>(
                    ir_kernel_arguments_address_array_i8ptr, ir_builder->getIndexConstant(i));
                auto ir_array_index_write = ir_builder->create<ir::WriteVariableLiked>(
                    ir_kernel_argument_address_i8ptr, ir_array_index);
            }

            auto ir_launch_function = lowering::symbolGet<ir::Value>(
                lowering::symbolGet<lowering::SymbolTable>(
                    ir_module->symbol_table->rootSymbolTable()->get("gpu"))
                    ->get("LaunchKernel"));
            PRAJNA_ASSERT(ir_launch_function);
            std::list<std::shared_ptr<ir::Value>> ir_arguments;
            ir_arguments.push_back(ir_builder->create<ir::BitCast>(
                ir_kernel_function, ir::PointerType::create(ir::IntType::create(8, true))));
            ir_arguments.push_back(ir_grid_shape);
            ir_arguments.push_back(ir_block_shape);
            auto ir_array_index0 = ir_builder->create<ir::IndexArray>(
                ir_kernel_arguments_address_array_i8ptr, ir_builder->getIndexConstant(0));
            auto ir_array_address = ir_builder->create<ir::BitCast>(
                ir_builder->create<ir::GetAddressOfVariableLiked>(ir_array_index0),
                ir::PointerType::create(ir::PointerType::create(ir::IntType::create(8, true))));
            ir_arguments.push_back(ir_array_address);
            ir_builder->create<ir::Call>(ir_launch_function, ir_arguments);

            utility::removeFromParent(ir_kernel_function_call);
            ir_kernel_function_call->finalize();
        }
    }
}

inline void convertKernelFunctionOperandToAddress(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_instructions = utility::getValuesInFunction<ir::Instruction>(ir_function);

        for (auto ir_instruction : ir_instructions) {
            for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
                if (auto ir_function = cast<ir::Function>(ir_instruction->operand(i))) {
                    if (ir_function->annotation_dict.count("kernel")) {
                        auto global_variable_fullname = getKernelFunctionAddressName(ir_function);

                        std::shared_ptr<ir::GlobalVariable> ir_global_variable = nullptr;
                        auto iter_global_variable =
                            std::find_if(RANGE(ir_module->global_variables),
                                         [=](std::shared_ptr<ir::GlobalVariable> x) {
                                             return x->fullname == global_variable_fullname;
                                         });
                        if (iter_global_variable != ir_module->global_variables.end()) {
                            ir_global_variable = *iter_global_variable;
                        } else {
                            ir_global_variable = ir::GlobalVariable::create(ir_function->type);
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

inline void convertGlobalVariableToPointer(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_global_variable : ir_module->global_variables) {
        auto ir_global_alloca = ir::GlobalAlloca::create(ir_global_variable->type);
        ir_global_alloca->name = ir_global_variable->name;
        ir_global_alloca->fullname = ir_global_variable->fullname;
        ir_global_alloca->parent_module = ir_module;
        // ir_global_variable->is_external默认为false
        ir_global_alloca->is_external = ir_global_variable->is_external;
        ir_module->global_allocas.push_back(ir_global_alloca);
    }

    ir_module->global_variables.clear();

    for (auto ir_function : ir_module->functions) {
        auto ir_instructions = utility::getValuesInFunction<ir::Instruction>(ir_function);
        for (auto ir_instruction : ir_instructions) {
            for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
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
                        ir_global_alloca = ir::GlobalAlloca::create(ir_global_variable->type);
                        ir_global_alloca->name = ir_global_variable->name;
                        ir_global_alloca->fullname = ir_global_variable->fullname;
                        ir_global_alloca->parent_module = ir_module;
                        ir_global_alloca->is_external = true;
                        ir_module->global_allocas.push_back(ir_global_alloca);
                    }

                    auto ir_deference_pointer = ir::DeferencePointer::create(ir_global_alloca);
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

inline void cloneExternalNvptxValue(std::shared_ptr<ir::Module> ir_module) {
    if (ir_module->modules.count(ir::Target::nvptx) == 0) {
    }

    auto ir_nvptx_module = ir_module->modules[ir::Target::nvptx];

    std::list<std::shared_ptr<ir::Function>> ir_kernel_functions_list;
    std::copy_if(RANGE(ir_nvptx_module->functions), std::back_inserter(ir_kernel_functions_list),
                 [](std::shared_ptr<ir::Function> ir_function) {
                     return ir_function->annotation_dict.count("kernel");
                 });

    PRAJNA_ASSERT(ir_kernel_functions_list.size() <= 1,
                  "两个以上后面重构, 每个核函数可能会有单独的module");

    if (ir_kernel_functions_list.empty()) return;

    auto ir_kernel_function = ir_kernel_functions_list.front();
    auto function_cloner = ir::FunctionCloner::create(ir_nvptx_module);
    // 会把生成的函数直接插入到module里
    ir_kernel_function->clone(function_cloner);
    // 移除原来的核函数
    ir_nvptx_module->functions.remove(ir_kernel_function);
}

inline void defineKernelFunctionAddress(std::shared_ptr<ir::Module> ir_module) {
    if (ir_module->modules.count(ir::Target::nvptx) == 0) {
    }

    auto ir_nvptx_module = ir_module->modules[ir::Target::nvptx];
    for (auto ir_function : ir_nvptx_module->functions) {
        if (ir_function->annotation_dict.count("kernel")) {
            auto global_variable_fullname = getKernelFunctionAddressName(ir_function);
            auto iter_global_variable = std::find_if(
                RANGE(ir_module->global_variables), [=](std::shared_ptr<ir::GlobalVariable> x) {
                    return x->fullname == global_variable_fullname;
                });
            if (iter_global_variable == ir_module->global_variables.end()) {
                auto ir_global_variable = ir::GlobalVariable::create(ir_function->type);
                ir_global_variable->name = global_variable_fullname;
                ir_global_variable->fullname = ir_global_variable->name;
                ir_global_variable->parent_module = ir_module;
                ir_module->global_variables.push_back(ir_global_variable);
            }
        }
    }
}

// @brief 移除return后面的指令. 若不移除, 会存在未知错误
inline void removeValuesAfterReturn(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        for (auto ir_block : ir_function->blocks) {
            auto iter_return = std::find_if(RANGE(ir_block->values), [](auto x) {
                return is<ir::Return>(x) || is<ir::JumpBranch>(x) || is<ir::ConditionBranch>(x);
            });
            if (iter_return != ir_block->values.end()) {
                ir_block->values.erase(std::next(iter_return), ir_block->values.end());
            }
        }
    }
}

inline void declareExternalFunction(std::shared_ptr<ir::Module> ir_module) {
    utility::each<ir::Instruction>(ir_module, [=](std::shared_ptr<ir::Instruction> ir_instruction) {
        for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
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
                        ir_decl_function = ir::Function::create(ir_function->function_type);
                        ir_decl_function->fullname = ir_function->fullname;
                        ir_decl_function->name = ir_function->name;
                        ir_decl_function->parent_module = ir_module;
                        ir_module->functions.push_front(ir_decl_function);
                    }

                    auto instruction_with_index_list_copy =
                        ir_function->instruction_with_index_list;
                    for (auto [ir_instruction, op_idx] : instruction_with_index_list_copy) {
                        if (ir_instruction->getParentFunction()->parent_module == ir_module) {
                            ir_instruction->operand(op_idx, ir_decl_function);
                        }
                    }
                }
            }
        }
    });
}

inline void convertForMultiDimToFor1Dim(std::shared_ptr<ir::Module> ir_module) {
    auto ir_fors = utility::getValuesInModule<ir::For>(ir_module);
    for (auto ir_for : ir_fors) {
        auto ir_builder = lowering::IrBuilder::create();
        ir_builder->symbol_table = ir_module->symbol_table;
        // 只需要对数组循环进行处理
        if (not ir_builder->isArrayIndexType(ir_for->index()->type)) continue;

        ir_builder->pushBlock(ir_for->parent_block);
        ir_builder->inserter_iterator = ir_for->parent_block->find(ir_for);
        auto ir_layout_template_struct = lowering::symbolGet<lowering::TemplateStruct>(
            ir_builder->getSymbolByPath(true, {"tensor", "Layout"}));
        PRAJNA_ASSERT(ir_layout_template_struct);
        auto ir_array_first = ir_for->first();
        auto ir_array_last = ir_for->last();
        auto ir_array_type = ir_array_last->type;
        auto ir_array_template_arguments =
            std::any_cast<std::list<lowering::Symbol>>(ir_array_type->template_arguments_any);
        auto ir_rank = lowering::symbolGet<ir::ConstantInt>(ir_array_template_arguments.back());
        std::list<lowering::Symbol> template_arguments = {ir_rank};
        auto ir_layout_type = ir_layout_template_struct->instantiate(template_arguments, ir_module);
        auto ir_layout =
            ir_builder->create<ir::Call>(ir_builder->GetImplementFunction(ir_layout_type, "Create"),
                                         std::list<std::shared_ptr<ir::Value>>{ir_for->last()});
        auto ir_linear_first = ir_builder->getIndexConstant(0);

        auto ir_array_one =
            ir_builder->create<ir::Call>(ir_builder->GetImplementFunction(ir_array_type, "One"));
        auto ir_array_range = ir_builder->callBinaryOperator(
            ir_builder->callBinaryOperator(ir_array_last, "-", ir_array_first), "-", ir_array_one);
        auto ir_linear_last = ir_builder->callBinaryOperator(
            ir_builder->callMemberFunction(ir_layout, "ArrayIndexToLinearIndex", {ir_array_range}),
            "+", ir_builder->getIndexConstant(1));

        ir_for->first(ir_linear_first);
        ir_for->last(ir_linear_last);
        auto ir_array_index = ir_for->index();
        auto ir_linear_index = ir_builder->create<ir::LocalVariable>(ir_builder->getIndexType());
        ir_for->index(ir_linear_index);

        auto ir_array_first_variable = ir_builder->variableLikedNormalize(ir_array_first);
        auto ir_layout_variable = ir_builder->variableLikedNormalize(ir_layout);
        ir_builder->pushBlock(ir_for->loopBlock());
        ir_builder->inserter_iterator = ir_for->loopBlock()->values.begin();
        utility::removeFromParent(ir_array_index);
        ir_builder->insert(ir_array_index);
        ir_builder->create<ir::WriteVariableLiked>(
            ir_builder->callBinaryOperator(
                ir_builder->callMemberFunction(ir_layout_variable, "LinearIndexToArrayIndex",
                                               {ir_linear_index}),
                "+", ir_array_first_variable),
            ir_array_index);
    }
}

inline void WrapIntrinsicFunction(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        if (ir_function->annotation_dict.count("intrinsic")) {
            PRAJNA_ASSERT(!ir_function->annotation_dict["intrinsic"].empty());
            auto intrinsic_function_name = ir_function->annotation_dict["intrinsic"].front();
            auto ir_decl_function = ir::Function::create(ir_function->function_type);
            ir_decl_function->fullname = intrinsic_function_name;
            ir_decl_function->parent_module = ir_module;
            ir_module->functions.push_front(ir_decl_function);

            PRAJNA_ASSERT(ir_function->blocks.empty());
            auto ir_builder = lowering::IrBuilder::create();
            ir_builder->createTopBlockForFunction(ir_function);
            ir_builder->create<ir::Return>(
                ir_builder->create<ir::Call>(ir_decl_function, ir_function->parameters));
            ir_function->annotation_dict.erase("intrinsic");
        }
    }

    for (auto [ir_target, ir_sub_module] : ir_module->modules) {
        if (not ir_module) continue;

        WrapIntrinsicFunction(ir_sub_module);
    }
}

inline void TopologicalSortFunctionVisit(
    std::shared_ptr<ir::Function> ir_function,
    std::list<std::shared_ptr<ir::Function>> &ir_function_list,
    std::set<std::shared_ptr<ir::Function>> &ir_gray_function_set) {
    // 标记要访问的函数
    ir_gray_function_set.insert(ir_function);
    auto ir_instructions = utility::getValuesInFunction<ir::Instruction>(ir_function);
    for (auto ir_instruction : ir_instructions) {
        for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
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
        auto ir_allocas = utility::getValuesInFunction<ir::Alloca>(ir_function);
        for (auto ir_alloca : ir_allocas) {
            if (ir_alloca->parent_block != ir_top_block) {
                utility::removeFromParent(ir_alloca);
                ir_top_block->pushFront(ir_alloca);
            }
        }
    }
}

inline std::shared_ptr<ir::Module> transform(std::shared_ptr<ir::Module> ir_module) {
    convertThisWrapperToDeferencePointer(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    convertForMultiDimToFor1Dim(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    convertPropertyToFunctionCall(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    insertReferenceCount(ir_module);
    TopologicalSortFunction(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    InlineFunction(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    PRAJNA_ASSERT(VerifyModule(ir_module));
    extractGpuFor(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    convertKernelFunctionCallToKernelLaunch(ir_module);
    flatternBlock(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    removeValuesAfterReturn(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    convertPropertyToFunctionCall(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    convertKernelFunctionOperandToAddress(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    convertGlobalVariableToPointer(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    convertThisWrapperToDeferencePointer(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    // ssa
    bool changed = true;
    while (changed) {
        changed = insertValueToBlock(ir_module);
        PRAJNA_ASSERT(VerifyModule(ir_module));
        changed = convertVariableToDeferencePointer(ir_module) || changed;
        PRAJNA_ASSERT(VerifyModule(ir_module));
        changed = convertAccessFieldToGetStructElementPointer(ir_module) || changed;
        PRAJNA_ASSERT(VerifyModule(ir_module));
        changed = convertIndexArrayToGetArrayElementPointer(ir_module) || changed;
        PRAJNA_ASSERT(VerifyModule(ir_module));
        changed = convertIndexPointerToGetPointerElementPointer(ir_module) || changed;
        PRAJNA_ASSERT(VerifyModule(ir_module));
        changed = convertGetAddressOfVaraibleLikedToPointer(ir_module) || changed;
        PRAJNA_ASSERT(VerifyModule(ir_module));
    }
    // 需要全部转为Deference才能进行, 因为上面的转换是围绕其进行的
    changed = convertDeferencePointerToStoreAndLoadPointer(ir_module) || changed;
    PRAJNA_ASSERT(VerifyModule(ir_module));
    //
    sperateModule(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    cloneExternalNvptxValue(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    defineKernelFunctionAddress(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    convertGlobalVariableToPointer(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    declareExternalFunction(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    WrapIntrinsicFunction(ir_module);
    PRAJNA_ASSERT(VerifyModule(ir_module));
    TopAlloca(ir_module);

    return ir_module;
}

}  // namespace prajna::transform
