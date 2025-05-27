#pragma once

#include <algorithm>
#include <memory>

#include "prajna/ir/ir.hpp"
#include "prajna/transform/each.hpp"

namespace prajna::transform::utility {

template <typename Value_>
inline std::list<std::shared_ptr<Value_>> GetAll(std::shared_ptr<ir::Value> ir_value) {
    std::list<std::shared_ptr<Value_>> ir_values;
    Each<ir::Value>(ir_value, [&ir_values](std::shared_ptr<ir::Value> ir_value) {
        if (auto ir_target_value = Cast<Value_>(ir_value)) {
            ir_values.push_back(ir_target_value);
        }
    });

    return ir_values;
}

inline void RemoveFromParent(std::shared_ptr<ir::Value> ir_value) {
    ir_value->GetParentBlock()->remove(ir_value);
}

inline void ReplaceInBlock(std::shared_ptr<ir::Value> ir_org, std::shared_ptr<ir::Value> ir_new) {
    auto ir_block = ir_org->GetParentBlock();
    auto iter = std::find(ir_block->begin(), ir_block->end(), ir_org);
    PRAJNA_ASSERT(iter != ir_block->end());
    ir_block->Insert(iter, ir_new);
    ir_block->Erase(iter);
    ir_org->Finalize();
}

inline std::list<std::shared_ptr<ir::Value>> CaptureExternalValueInClosure(
    std::shared_ptr<ir::Function> ir_function) {
    std::list<std::shared_ptr<ir::Value>> ir_values;

    if (!ir_function->closure) return ir_values;

    Each<ir::Value>(ir_function, [&](std::shared_ptr<ir::Value> ir_value) {
        if (auto ir_instruction = Cast<ir::Instruction>(ir_value)) {
            for (int64_t i = 0; i < ir_instruction->OperandSize(); ++i) {
                auto ir_operand = ir_instruction->GetOperand(i);
                if (ir::IsGlobal(ir_operand)) continue;
                if (ir_operand->GetParentFunction() != ir_function) {
                    ir_values.push_back(ir_operand);
                }
            }
        }
        // TODO: nested closure
    });

    return ir_values;
}

inline std::list<std::shared_ptr<ir::Variable>> CaptureExternalVariablesInBlock(
    std::shared_ptr<ir::Block> ir_block) {
    std::list<std::shared_ptr<ir::Variable>> ir_variables;
    for (auto ir_value : *ir_block) {
        // 先判断是否是ir::For, 因为ir::For本身也是ir::Instruction
        PRAJNA_ASSERT(!Is<ir::For>(ir_value));
        if (auto ir_instruction = Cast<ir::Instruction>(ir_value)) {
            for (int64_t i = 0; i < ir_instruction->OperandSize(); ++i) {
                auto ir_operand = ir_instruction->GetOperand(i);
                if (ir_operand->GetParentBlock() != ir_block) {
                    if (auto ir_local_variable = Cast<ir::Variable>(ir_operand)) {
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
    if (auto ir_struct_type = Cast<ir::StructType>(ir_type)) {
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
    if (auto ir_access_field = Cast<ir::AccessField>(ir_variable_liked)) {
        return IsReferedTo(ir_refered_variable, ir_access_field->object());
    }
    if (auto ir_index_array = Cast<ir::IndexArray>(ir_variable_liked)) {
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
                   [](auto x) { return ir::StringToTarget(x); });
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
