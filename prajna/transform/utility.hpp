#pragma once

#include <algorithm>
#include <memory>

#include "prajna/ir/ir.hpp"

namespace prajna::transform::utility {

// auto create = [&](auto )
struct IrBuilder {
    template <typename _Value, typename... _Args>
    std::shared_ptr<_Value> create(_Args &&... __args) {
        auto ir_value = _Value::create(std::forward<_Args>(__args)...);
        static_assert(std::is_base_of<ir::Value, _Value>::value);
        PRAJNA_ASSERT(ir_block);
        ir_block->insert(iter, ir_value);
        if (this->create_callback) {
            this->create_callback(ir_value);
        }
        return ir_value;
    }

    std::shared_ptr<ir::VariableLiked> variableLikedNormalize(std::shared_ptr<ir::Value> ir_value) {
        auto ir_variable_liked = cast<ir::VariableLiked>(ir_value);
        if (ir_variable_liked) {
            return ir_variable_liked;
        } else {
            ir_variable_liked = this->create<ir::LocalVariable>(ir_value->type);
            auto ir_write = this->create<ir::WriteVariableLiked>(ir_value, ir_variable_liked);
        }
        return ir_variable_liked;
    }

    std::shared_ptr<ir::Call> callMemberFunction(
        std::shared_ptr<ir::Value> ir_object, std::string member_function,
        std::vector<std::shared_ptr<ir::Value>> ir_arguments) {
        auto ir_variable_liked = this->variableLikedNormalize(ir_object);
        auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        ir_arguments.insert(ir_arguments.begin(), ir_this_pointer);
        auto ir_member_function = ir_object->type->member_functions[member_function];
        PRAJNA_ASSERT(ir_member_function);
        return this->create<ir::Call>(ir_member_function, ir_arguments);
    }

    std::shared_ptr<ir::Block> ir_block;
    ir::Block::iterator iter;
    std::function<void(std::shared_ptr<ir::Value>)> create_callback;
};

inline void eachValue(std::shared_ptr<ir::Block> ir_block,
                      std::function<void(std::shared_ptr<ir::Value>)> ir_value_callback) {
    for (auto &ir_value : ir_block->values) {
        ir_value_callback(ir_value);

        if (auto ir_block = cast<ir::Block>(ir_value)) {
            eachValue(ir_block, ir_value_callback);
        }

        if (auto ir_if = cast<ir::If>(ir_value)) {
            eachValue(ir_if->trueBlock(), ir_value_callback);
            eachValue(ir_if->falseBlock(), ir_value_callback);
        }

        if (auto ir_while = cast<ir::While>(ir_value)) {
            eachValue(ir_while->loopBlock(), ir_value_callback);
        }

        if (auto ir_for = cast<ir::For>(ir_value)) {
            eachValue(ir_for->loopBlock(), ir_value_callback);
        }
    }
}

inline void eachValue(std::shared_ptr<ir::Function> ir_function,
                      std::function<void(std::shared_ptr<ir::Value>)> ir_value_callback) {
    for (auto &ir_block : ir_function->blocks) {
        eachValue(ir_block, ir_value_callback);
    }
}

template <typename _Value>
inline void each(std::shared_ptr<ir::Function> ir_function,
                 std::function<void(std::shared_ptr<_Value>)> ir_value_callback) {
    for (auto &ir_block : ir_function->blocks) {
        for (auto &ir_value : ir_block->values) {
            PRAJNA_ASSERT(!is<ir::Block>(ir_value), "Block嵌套");
            if (is<_Value>(ir_value)) {
                ir_value_callback(ir_value);
            }
        }
    }
}

template <typename _Value>
inline std::list<std::shared_ptr<_Value>> getValuesInFunction(
    std::shared_ptr<ir::Function> ir_function) {
    std::list<std::shared_ptr<_Value>> ir_values;
    utility::eachValue(ir_function, [&](std::shared_ptr<ir::Value> ir_value) {
        if (auto ir_variable = cast<_Value>(ir_value)) {
            ir_values.push_back(ir_variable);
        }
    });

    return ir_values;
}

inline void removeFromParent(std::shared_ptr<ir::Value> ir_value) {
    ir_value->parent_block->remove(ir_value);
}

inline void replaceInBlock(std::shared_ptr<ir::Value> ir_org, std::shared_ptr<ir::Value> ir_new) {
    PRAJNA_ASSERT(ir_org->parent_block);
    auto ir_block = ir_org->parent_block;
    auto iter = std::find(ir_block->values.begin(), ir_block->values.end(), ir_org);
    ir_block->insert(iter, ir_new);
    ir_block->erase(iter);
}

inline std::list<std::shared_ptr<ir::LocalVariable>> captureExternalVariablesInBlock(
    std::shared_ptr<ir::Block> ir_block) {
    std::list<std::shared_ptr<ir::LocalVariable>> ir_variables;
    for (auto ir_value : ir_block->values) {
        // 先判断是否是ir::For, 因为ir::Grid本身也是ir::Instruction
        PRAJNA_ASSERT(!is<ir::For>(ir_value));
        if (auto ir_instruction = cast<ir::Instruction>(ir_value)) {
            for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
                auto ir_operand = ir_instruction->operand(i);
                if (auto ir_variable = cast<ir::LocalVariable>(ir_operand)) {
                    if (ir_variable->parent_block != ir_block) {
                        ir_variables.push_back(ir_variable);
                    }
                }
            }
            continue;
        }
    }

    return ir_variables;
}

inline bool isGeneralCapturedType(std::shared_ptr<ir::Type> ir_type) {
    if (is<ir::PointerType>(ir_type)) {
        return false;
    }
    if (auto ir_struct_type = cast<ir::StructType>(ir_type)) {
        for (auto field : ir_struct_type->fields) {
            if (!isGeneralCapturedType(field->type)) {
                return false;
            }
        }
    }

    return true;
}

inline bool isTensorType(std::shared_ptr<ir::Type> ir_type) {
    std::string tensor_fullname_prefix = "::core::Tensor<";
    if (ir_type->fullname.size() > tensor_fullname_prefix.size()) {
        if (std::equal(RANGE(tensor_fullname_prefix), ir_type->fullname.begin())) {
            if (ir_type->fullname.back() == '>') {
                return true;
            }
        }
    }

    return false;
}

inline bool isCapturedType(std::shared_ptr<ir::Type> ir_type) {
    return isGeneralCapturedType(ir_type) || utility::isTensorType(ir_type);
}

inline bool isReferedTo(std::shared_ptr<ir::LocalVariable> ir_refered_variable,
                        std::shared_ptr<ir::Value> ir_variable_liked) {
    if (ir_refered_variable == ir_variable_liked) {
        return true;
    }
    if (auto ir_access_field = cast<ir::AccessField>(ir_variable_liked)) {
        return isReferedTo(ir_refered_variable, ir_access_field->object());
    }
    if (auto ir_index_array = cast<ir::IndexArray>(ir_variable_liked)) {
        return isReferedTo(ir_refered_variable, ir_index_array->object());
    }

    return false;
}

inline std::list<ir::Target> getTargets(std::shared_ptr<ir::Function> ir_function) {
    auto target_string_vector = ir_function->function_type->annotations["target"];
    std::list<std::string> target_string_list(target_string_vector.begin(),
                                              target_string_vector.end());

    std::list<ir::Target> ir_target_list;
    std::transform(RANGE(target_string_list), std::inserter(ir_target_list, ir_target_list.begin()),
                   [](auto x) { return ir::stringToTarget(x); });
    return ir_target_list;
}

}  // namespace prajna::transform::utility
