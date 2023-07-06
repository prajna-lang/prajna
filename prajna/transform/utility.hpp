#pragma once

#include <algorithm>
#include <memory>

#include "prajna/ir/ir.hpp"

namespace prajna::transform::utility {

inline void EachValue(std::shared_ptr<ir::Block> ir_block,
                      std::function<void(std::shared_ptr<ir::Value>)> callback) {
    for (auto &ir_value : ir_block->values) {
        callback(ir_value);

        if (auto ir_block = cast<ir::Block>(ir_value)) {
            EachValue(ir_block, callback);
        }

        if (auto ir_if = cast<ir::If>(ir_value)) {
            EachValue(ir_if->TrueBlock(), callback);
            EachValue(ir_if->FalseBlock(), callback);
        }

        if (auto ir_while = cast<ir::While>(ir_value)) {
            EachValue(ir_while->ConditionBlock(), callback);
            EachValue(ir_while->LoopBlock(), callback);
        }

        if (auto ir_for = cast<ir::For>(ir_value)) {
            EachValue(ir_for->LoopBlock(), callback);
        }
    }

    callback(ir_block);
}

inline void EachValue(std::shared_ptr<ir::Function> ir_function,
                      std::function<void(std::shared_ptr<ir::Value>)> callback) {
    for (auto &ir_block : ir_function->blocks) {
        EachValue(ir_block, callback);
    }

    callback(ir_function);
}

inline void EachValue(std::shared_ptr<ir::Module> ir_module,
                      std::function<void(std::shared_ptr<ir::Value>)> callback) {
    for (auto ir_value : ir_module->global_variables) {
        callback(ir_value);
    }

    for (auto ir_value : ir_module->global_allocas) {
        callback(ir_value);
    }

    for (auto ir_function : ir_module->functions) {
        EachValue(ir_function, callback);
    }
}

template <typename Value_>
inline void Each(std::shared_ptr<ir::Module> ir_module,
                 std::function<void(std::shared_ptr<Value_>)> ir_callback) {
    EachValue(ir_module, [=](auto ir_e) {
        if (auto ir_target = cast<Value_>(ir_e)) {
            ir_callback(ir_target);
        }
    });
}

template <typename Value_>
inline std::list<std::shared_ptr<Value_>> GetValuesInFunction(
    std::shared_ptr<ir::Function> ir_function) {
    std::list<std::shared_ptr<Value_>> ir_values;
    utility::EachValue(ir_function, [&ir_values](std::shared_ptr<ir::Value> ir_value) {
        if (auto ir_target_value = cast<Value_>(ir_value)) {
            ir_values.push_back(ir_target_value);
        }
    });

    return ir_values;
}

template <typename Value_>
inline std::list<std::shared_ptr<Value_>> GetValuesInModule(std::shared_ptr<ir::Module> ir_module) {
    std::list<std::shared_ptr<Value_>> ir_values;
    EachValue(ir_module, [&ir_values](std::shared_ptr<ir::Value> ir_value) {
        if (auto ir_target_value = cast<Value_>(ir_value)) {
            ir_values.push_back(ir_target_value);
        }
    });

    return ir_values;
}

inline void RemoveFromParent(std::shared_ptr<ir::Value> ir_value) {
    ir_value->parent_block->remove(ir_value);
}

inline void ReplaceInBlock(std::shared_ptr<ir::Value> ir_org, std::shared_ptr<ir::Value> ir_new) {
    PRAJNA_ASSERT(ir_org->parent_block);
    auto ir_block = ir_org->parent_block;
    auto iter = std::find(ir_block->values.begin(), ir_block->values.end(), ir_org);
    PRAJNA_ASSERT(iter != ir_block->values.end());
    ir_block->insert(iter, ir_new);
    ir_block->erase(iter);
    ir_org->Finalize();
}

inline std::list<std::shared_ptr<ir::Variable>> CaptureExternalVariablesInBlock(
    std::shared_ptr<ir::Block> ir_block) {
    std::list<std::shared_ptr<ir::Variable>> ir_variables;
    for (auto ir_value : ir_block->values) {
        // 先判断是否是ir::For, 因为ir::For本身也是ir::Instruction
        PRAJNA_ASSERT(!Is<ir::For>(ir_value));
        if (auto ir_instruction = cast<ir::Instruction>(ir_value)) {
            for (size_t i = 0; i < ir_instruction->OperandSize(); ++i) {
                auto ir_operand = ir_instruction->operand(i);
                if (ir_operand->parent_block != ir_block) {
                    if (auto ir_local_variable = cast<ir::Variable>(ir_operand)) {
                        ir_variables.push_back(ir_local_variable);
                    }
                }
            }
            continue;
        }
    }

    return ir_variables;
}

inline bool IsGeneralCapturedType(std::shared_ptr<ir::Type> ir_type) {
    if (Is<ir::PointerType>(ir_type)) {
        return false;
    }
    if (auto ir_struct_type = cast<ir::StructType>(ir_type)) {
        for (auto field : ir_struct_type->fields) {
            if (!IsGeneralCapturedType(field->type)) {
                return false;
            }
        }
    }

    return true;
}

inline bool IsTensorType(std::shared_ptr<ir::Type> ir_type) {
    PRAJNA_TODO;
    return false;
    // auto gpu_tensor_template_struct = lowering::SymbolGet<lowering::TemplateStruct>(
    //     ir_builder->GetSymbolByPath(true, {"gpu", "Tensor"}));
    // auto host_tensor_template_struct = lowering::SymbolGet<lowering::TemplateStruct>(
    //     ir_builder->GetSymbolByPath(true, {"Tensor"}));

    // return ir_type->template_str == gpu_tensor_template_struct ||
    //        ir_type->template_arguments == host_tensor_template_struct;
}

inline bool IsCapturedType(std::shared_ptr<ir::Type> ir_type) {
    return IsGeneralCapturedType(ir_type) || utility::IsTensorType(ir_type);
}

inline bool IsReferedTo(std::shared_ptr<ir::LocalVariable> ir_refered_variable,
                        std::shared_ptr<ir::Value> ir_variable_liked) {
    if (ir_refered_variable == ir_variable_liked) {
        return true;
    }
    if (auto ir_access_field = cast<ir::AccessField>(ir_variable_liked)) {
        return IsReferedTo(ir_refered_variable, ir_access_field->object());
    }
    if (auto ir_index_array = cast<ir::IndexArray>(ir_variable_liked)) {
        return IsReferedTo(ir_refered_variable, ir_index_array->object());
    }

    return false;
}

inline std::list<ir::Target> GetTargets(std::shared_ptr<ir::Function> ir_function) {
    auto target_string_vector = ir_function->annotation_dict["target"];
    std::list<std::string> target_string_list(target_string_vector.begin(),
                                              target_string_vector.end());

    std::list<ir::Target> ir_target_list;
    std::transform(RANGE(target_string_list), std::inserter(ir_target_list, ir_target_list.begin()),
                   [](auto x) { return ir::stringToTarget(x); });
    return ir_target_list;
}

inline bool IsHostTensorType(std::shared_ptr<ir::Type> ir_type,
                             std::shared_ptr<ir::Module> ir_module) {
    auto ir_builder = lowering::IrBuilder::Create(ir_module->symbol_table, nullptr, nullptr);
    // 需要更新为固定路径
    auto host_tensor_template_struct = lowering::SymbolGet<lowering::TemplateStruct>(
        ir_builder->GetSymbolByPath(true, {"Tensor"}));
    PRAJNA_ASSERT(host_tensor_template_struct);
    PRAJNA_ASSERT(ir_type->template_struct);
    return ir_type->template_struct == host_tensor_template_struct;
}

inline bool IsGpuTensorType(std::shared_ptr<ir::Type> ir_type,
                            std::shared_ptr<ir::Module> ir_module) {
    auto ir_builder = lowering::IrBuilder::Create(ir_module->symbol_table, nullptr, nullptr);
    // 需要更新为固定路径
    auto gpu_tensor_template_struct = lowering::SymbolGet<lowering::TemplateStruct>(
        ir_builder->GetSymbolByPath(true, {"gpu", "Tensor"}));
    PRAJNA_ASSERT(gpu_tensor_template_struct);
    PRAJNA_ASSERT(ir_type->template_struct);
    return ir_type->template_struct == gpu_tensor_template_struct;
}

inline std::shared_ptr<ir::Type> GetGpuTensorTypeOfHostTensorType(
    std::shared_ptr<ir::Type> ir_type, std::shared_ptr<ir::Module> ir_module) {
    auto ir_builder = lowering::IrBuilder::Create(ir_module->symbol_table, nullptr, nullptr);
    // 需要更新为固定路径
    auto gpu_tensor_template_struct = lowering::SymbolGet<lowering::TemplateStruct>(
        ir_builder->GetSymbolByPath(true, {"gpu", "Tensor"}));
    PRAJNA_ASSERT(gpu_tensor_template_struct);
    PRAJNA_ASSERT(ir_type->template_struct);
    return gpu_tensor_template_struct->Instantiate(
        std::any_cast<std::list<lowering::Symbol>>(ir_type->template_arguments_any), ir_module);
}

}  // namespace prajna::transform::utility
