#pragma once

#include <memory>
#include <stack>

#include "prajna/ir/ir.hpp"
#include "prajna/logger.hpp"
#include "prajna/lowering/statement_lowering_visitor.hpp"
#include "prajna/parser/parse.h"
#include "prajna/transform/extract_gpu_grid_pass.hpp"
#include "prajna/transform/flattern_block.hpp"
#include "prajna/transform/initializer_and_copy_destroy_callback.hpp"
#include "prajna/transform/transform_pass.hpp"
#include "prajna/transform/utility.hpp"
#include "prajna/transform/verify.hpp"
#include "prajna/transform/wrap_instruction_function_pass.hpp"

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

// auto create = [=](auto )
inline std::shared_ptr<ir::Module> convertPropertyToFunctionCall(
    std::shared_ptr<ir::Module> ir_module) {
    auto ir_access_properties = utility::getValuesInModule<ir::AccessProperty>(ir_module);
    for (auto ir_access_property : ir_access_properties) {
        auto ir_block = ir_access_property->parent_block;
        auto ir_builder = lowering::IrBuilder::create();
        ir_builder->current_block = ir_block;
        ir_builder->inserter_iterator = std::find(RANGE(ir_block->values), ir_access_property);

        auto instructions_with_index_set_copy = ir_access_property->instruction_with_index_list;

        for (auto instruction_with_index : instructions_with_index_set_copy) {
            auto ir_inst = instruction_with_index.instruction;
            size_t op_idx = instruction_with_index.operand_index;

            PRAJNA_ASSERT(not is<ir::GetAddressOfVariableLiked>(ir_inst));

            if (is<ir::WriteProperty>(ir_inst) && op_idx == 1) {
                auto ir_write_property = cast<ir::WriteProperty>(ir_inst);
                auto ir_arguments = ir_access_property->arguments();
                ir_arguments.insert(ir_arguments.begin(), ir_access_property->thisPointer());
                ir_arguments.push_back(ir_write_property->value());
                auto ir_setter_call = ir_builder->create<ir::Call>(
                    ir_access_property->property->setter_function, ir_arguments);
                utility::removeFromParent(ir_write_property);
                ir_write_property->finalize();
            } else {
                auto ir_arguments = ir_access_property->arguments();
                ir_arguments.insert(ir_arguments.begin(), ir_access_property->thisPointer());
                auto ir_getter_call = ir_builder->create<ir::Call>(
                    ir_access_property->property->getter_function, ir_arguments);
                ir_inst->operand(op_idx, ir_getter_call);
            }
        }

        if (instructions_with_index_set_copy.empty()) {
            auto ir_arguments = ir_access_property->arguments();
            ir_arguments.insert(ir_arguments.begin(), ir_access_property->thisPointer());
            auto ir_getter_call = ir_builder->create<ir::Call>(
                ir_access_property->property->getter_function, ir_arguments);
        }

        utility::removeFromParent(ir_access_property);
        ir_access_property->finalize();
    }

    for (auto [ir_target, ir_sub_module] : ir_module->modules) {
        if (not ir_module) continue;

        convertPropertyToFunctionCall(ir_sub_module);
    }

    return ir_module;
}

inline std::shared_ptr<ir::Module> convertKernelFunctionCallToKernelLaunch(
    std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_kernel_function_calls =
            utility::getValuesInFunction<ir::KernelFunctionCall>(ir_function);
        for (auto ir_kernel_function_call : ir_kernel_function_calls) {
            auto ir_kernel_function = ir_kernel_function_call->function();
            auto ir_grid_shape = ir_kernel_function_call->gridShape();
            auto ir_block_shape = ir_kernel_function_call->blockShape();
            // auto ir_arguments = ir_kernel_function_call->arguments();
            auto ir_block = ir_kernel_function_call->parent_block;

            auto ir_builder = lowering::IrBuilder::create();
            ir_builder->current_block = ir_block;
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
                    ->get("launchKernel"));
            PRAJNA_ASSERT(ir_launch_function);
            std::vector<std::shared_ptr<ir::Value>> ir_arguments(4);
            ir_arguments[0] = ir_builder->create<ir::BitCast>(
                ir_kernel_function, ir::PointerType::create(ir::IntType::create(8, true)));
            ir_arguments[1] = ir_grid_shape;
            ir_arguments[2] = ir_block_shape;
            auto ir_array_index0 = ir_builder->create<ir::IndexArray>(
                ir_kernel_arguments_address_array_i8ptr, ir_builder->getIndexConstant(0));
            auto ir_array_address = ir_builder->create<ir::BitCast>(
                ir_builder->create<ir::GetAddressOfVariableLiked>(ir_array_index0),
                ir::PointerType::create(ir::PointerType::create(ir::IntType::create(8, true))));
            ir_arguments[3] = ir_array_address;
            ir_builder->create<ir::Call>(ir_launch_function, ir_arguments);

            utility::removeFromParent(ir_kernel_function_call);
            ir_kernel_function_call->finalize();
        }
    }

    return ir_module;
}

inline std::shared_ptr<ir::Module> convertKernelFunctionOperandToAddress(
    std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_instructions = utility::getValuesInFunction<ir::Instruction>(ir_function);

        for (auto ir_instruction : ir_instructions) {
            for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
                if (auto ir_function = cast<ir::Function>(ir_instruction->operand(i))) {
                    if (ir_function->annotations.count("kernel")) {
                        auto global_variable_fullname =
                            getKernelFunctionAddressName(ir_function->function_type->fullname);

                        sp<ir::GlobalVariable> ir_global_variable = nullptr;
                        auto iter_global_variable = std::find_if(
                            RANGE(ir_module->global_variables), [=](sp<ir::GlobalVariable> x) {
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
    return ir_module;
}

inline sp<ir::Module> convertGlobalVariableToPointer(sp<ir::Module> ir_module) {
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
                    auto iter_global_alloca =
                        std::find_if(RANGE(ir_module->global_allocas), [=](sp<ir::GlobalAlloca> x) {
                            return x->fullname == ir_global_variable->fullname;
                        });
                    sp<ir::GlobalAlloca> ir_global_alloca = nullptr;
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

    return ir_module;
}

inline std::shared_ptr<ir::Module> cloneExternalNvptxValue(std::shared_ptr<ir::Module> ir_module) {
    if (ir_module->modules.count(ir::Target::nvptx) == 0) {
        return ir_module;
    }

    auto ir_nvptx_module = ir_module->modules[ir::Target::nvptx];

    std::list<std::shared_ptr<ir::Function>> ir_kernel_functions_list;
    std::copy_if(RANGE(ir_nvptx_module->functions), std::back_inserter(ir_kernel_functions_list),
                 [](std::shared_ptr<ir::Function> ir_function) {
                     return ir_function->annotations.count("kernel");
                 });

    PRAJNA_ASSERT(ir_kernel_functions_list.size() <= 1,
                  "两个以上后面重构, 每个核函数可能会有单独的module");

    if (ir_kernel_functions_list.empty()) return ir_module;

    auto ir_kernel_function = ir_kernel_functions_list.front();
    auto function_cloner = ir::FunctionCloner::create(ir_nvptx_module);
    ir_kernel_function->clone(function_cloner);

    ir_nvptx_module->functions = function_cloner->functions;

    return ir_module;
}

inline std::shared_ptr<ir::Module> defineKernelFunctionAddress(
    std::shared_ptr<ir::Module> ir_module) {
    if (ir_module->modules.count(ir::Target::nvptx) == 0) {
        return ir_module;
    }

    auto ir_nvptx_module = ir_module->modules[ir::Target::nvptx];
    for (auto ir_function : ir_nvptx_module->functions) {
        if (ir_function->annotations.count("kernel")) {
            auto global_variable_fullname =
                getKernelFunctionAddressName(ir_function->function_type->fullname);
            auto iter_global_variable = std::find_if(
                RANGE(ir_module->global_variables),
                [=](sp<ir::GlobalVariable> x) { return x->fullname == global_variable_fullname; });
            if (iter_global_variable == ir_module->global_variables.end()) {
                auto ir_global_variable = ir::GlobalVariable::create(ir_function->type);
                ir_global_variable->name = global_variable_fullname;
                ir_global_variable->fullname = ir_global_variable->name;
                ir_global_variable->parent_module = ir_module;
                ir_module->global_variables.push_back(ir_global_variable);
            }
        }
    }

    return ir_module;
}

// @brief 移除return后面的指令. 若不移除, 会存在未知错误
inline std::shared_ptr<ir::Module> removeValuesAfterReturn(std::shared_ptr<ir::Module> ir_module) {
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

    return ir_module;
}

inline std::shared_ptr<ir::Module> declareExternalFunction(std::shared_ptr<ir::Module> ir_module) {
    utility::each<ir::Call>(ir_module, [=](std::shared_ptr<ir::Call> ir_call) {
        if (auto ir_callee = cast<ir::Function>(ir_call->function())) {
            if (ir_callee->parent_module != ir_module && !ir_callee->isInstruction()) {
                // 不会重复添加, 因为会置换所有的操作数
                auto ir_function = ir::Function::create(ir_callee->function_type);
                ir_function->fullname = ir_callee->fullname;
                ir_function->name = ir_callee->name;
                ir_function->parent_module = ir_module;
                ir_module->functions.push_front(ir_function);

                auto instruction_with_index_list_copy = ir_callee->instruction_with_index_list;
                for (auto [ir_instruction, op_idx] : instruction_with_index_list_copy) {
                    if (ir_instruction->getParentFunction()->parent_module == ir_module) {
                        ir_instruction->operand(op_idx, ir_function);
                    }
                }
            }
        }
    });

    return ir_module;
}

inline std::shared_ptr<ir::Module> convertForMultiDimToFor1Dim(
    std::shared_ptr<ir::Module> ir_module) {
    auto ir_fors = utility::getValuesInModule<ir::For>(ir_module);
    for (auto ir_for : ir_fors) {
        auto ir_builder = lowering::IrBuilder::create();
        ir_builder->symbol_table = ir_module->symbol_table;
        // 不是数组索引的for, 不做处理
        if (not ir_builder->isArrayIndexType(ir_for->index()->type)) continue;

        ir_builder->current_block = ir_for->parent_block;
        ir_builder->inserter_iterator = ir_for->parent_block->find(ir_for);
        // fisrt =
        auto ir_layout_template_struct = lowering::symbolGet<lowering::TemplateStruct>(
            ir_builder->getSymbolByPath(true, {"tensor", "Layout"}));
        PRAJNA_ASSERT(ir_layout_template_struct);
        auto ir_array_first = ir_for->first();
        auto ir_array_last = ir_for->last();
        auto ir_array_type = ir_array_last->type;
        // @todo 需要修复, 先设置为2
        auto ir_array_template_arguments =
            std::any_cast<std::list<lowering::Symbol>>(ir_array_type->template_arguments);
        auto ir_rank = lowering::symbolGet<ir::ConstantInt>(ir_array_template_arguments.back());
        std::list<lowering::Symbol> template_arguments = {ir_rank};
        auto ir_layout_type =
            ir_layout_template_struct->getStructInstance(template_arguments, ir_module);
        auto ir_layout =
            ir_builder->create<ir::Call>(ir_layout_type->static_functions["create"],
                                         std::vector<std::shared_ptr<ir::Value>>{ir_for->last()});
        auto ir_linear_first = ir_builder->getIndexConstant(0);

        auto ir_array_one = ir_builder->create<ir::Call>(ir_array_type->static_functions["one"]);
        auto ir_array_range = ir_builder->callBinaryOperator(
            ir_builder->callBinaryOperator(ir_array_last, "-", ir_array_first), "-", ir_array_one);
        auto ir_linear_last = ir_builder->callBinaryOperator(
            ir_builder->callMemberFunction(ir_layout, "arrayIndexToLinearIndex", {ir_array_range}),
            "+", ir_builder->getIndexConstant(1));

        ir_for->first(ir_linear_first);
        ir_for->last(ir_linear_last);
        auto ir_array_index = ir_for->index();
        auto ir_linear_index = ir_builder->create<ir::LocalVariable>(ir_builder->getIndexType());
        ir_for->index(ir_linear_index);

        ir_builder->current_block = ir_for->loopBlock();
        ir_builder->inserter_iterator = ir_for->loopBlock()->values.begin();
        ir_builder->create<ir::WriteVariableLiked>(
            ir_builder->callBinaryOperator(
                ir_builder->callMemberFunction(ir_layout, "linearIndexToArrayIndex",
                                               {ir_linear_index}),
                "+", ir_array_first),
            ir_array_index);
    }

    return ir_module;
}

inline std::shared_ptr<ir::Module> transform(std::shared_ptr<ir::Module> ir_module) {
    ir_module = convertForMultiDimToFor1Dim(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = convertPropertyToFunctionCall(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = wrapInstructionFunction(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = insertInitializeAndCopyAndDestroyCallback(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = flatternBlock(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = removeValuesAfterReturn(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = extractGpuFor(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = convertKernelFunctionCallToKernelLaunch(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = flatternBlock(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = convertPropertyToFunctionCall(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = convertKernelFunctionOperandToAddress(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = convertGlobalVariableToPointer(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = convertVariableToPointer(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = sperateModule(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = cloneExternalNvptxValue(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = defineKernelFunctionAddress(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = convertGlobalVariableToPointer(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    // 在sperateModule后面
    PRAJNA_ASSERT(verifyTree(ir_module));
    ir_module = declareExternalFunction(ir_module);
    PRAJNA_ASSERT(verifyTree(ir_module));
    return ir_module;
}
}  // namespace prajna::transform
