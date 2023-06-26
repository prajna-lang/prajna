#pragma once

#include <list>
#include <set>
#include <unordered_map>

#include "prajna/ir/ir.hpp"
#include "prajna/logger.hpp"
#include "prajna/lowering/statement_lowering_visitor.hpp"
#include "prajna/parser/parse.h"
#include "prajna/transform/transform_pass.hpp"
#include "prajna/transform/utility.hpp"

namespace prajna::transform {

inline bool insertValueToBlock(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;
    for (auto ir_function : ir_module->functions) {
        std::set<std::shared_ptr<ir::Value>> ir_values;
        for (auto ir_block : ir_function->blocks) {
            for (auto iter_ir_value = ir_block->values.begin();
                 iter_ir_value != ir_block->values.end(); ++iter_ir_value) {
                auto ir_value = *iter_ir_value;
                ir_values.insert(ir_value);

                if (auto ir_instruction = cast<ir::Instruction>(ir_value)) {
                    for (auto i = 0; i < ir_instruction->operandSize(); ++i) {
                        auto ir_operand = ir_instruction->operand(i);
                        if (ir_values.count(ir_operand) == 0) {
                            // 模板实例化时会产生非Block的Constant,并且会被使用
                            if (is<ir::ConstantInt>(ir_operand)) {
                                changed = true;
                                ir_block->insert(iter_ir_value, ir_operand);
                                ir_values.insert(ir_operand);
                            }
                        }
                    }
                }
            }
        }
    }

    return changed;
}

inline bool convertThisWrapperToDeferencePointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;
    for (auto ir_function : ir_module->functions) {
        //
        auto ir_this_wrappers = utility::getValuesInFunction<ir::ThisWrapper>(ir_function);

        for (auto ir_this_wrapper : ir_this_wrappers) {
            // 改变使用它的
            changed = true;
            auto ir_deference_pointer =
                ir::DeferencePointer::create(ir_this_wrapper->thisPointer());
            auto iter = std::find(RANGE(ir_this_wrapper->parent_block->values), ir_this_wrapper);
            ir_this_wrapper->parent_block->insert(iter, ir_deference_pointer);
            for (auto instruction_with_index_list :
                 clone(ir_this_wrapper->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                size_t op_idx = instruction_with_index_list.operand_index;

                auto ir_block = ir_inst->parent_block;
                ir_inst->operand(op_idx, ir_deference_pointer);
            }

            ir_this_wrapper->parent_block->remove(ir_this_wrapper);
            ir_this_wrapper->finalize();
        }
    }

    return changed;
}

inline bool convertVariableToDeferencePointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;

    for (auto ir_function : ir_module->functions) {
        //
        auto ir_variables = utility::getValuesInFunction<ir::LocalVariable>(ir_function);

        for (auto ir_variable : ir_variables) {
            //改变它自己
            changed = true;
            std::shared_ptr<ir::Value> ir_alloca = ir::Alloca::create(ir_variable->type);
            ir_alloca->name = ir_variable->name;
            ir_alloca->fullname = ir_alloca->name;

            // @note ir::Alloca不应该出现在循环体内部, 故直接再将其放在函数的第一个块
            ir_function->blocks.front()->pushFront(ir_alloca);
            ir_variable->parent_block->remove(ir_variable);

            // 改变使用它的
            for (auto instruction_with_index_list :
                 clone(ir_variable->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                size_t op_idx = instruction_with_index_list.operand_index;

                auto ir_deference_pointer = ir::DeferencePointer::create(ir_alloca);
                auto ir_block = ir_inst->parent_block;
                auto iter = std::find(ir_block->values.begin(), ir_block->values.end(), ir_inst);
                ir_block->insert(iter, ir_deference_pointer);
                ir_inst->operand(op_idx, ir_deference_pointer);
            }

            ir_variable->finalize();
        }
    }

    return changed;
}

inline bool convertAccessFieldToGetStructElementPointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;
    for (auto ir_function : ir_module->functions) {
        auto ir_access_fields = utility::getValuesInFunction<ir::AccessField>(ir_function);

        for (auto ir_access_field : ir_access_fields) {
            auto ir_object_deference_ptr0 = (ir_access_field->object());
            auto ir_object_deference_ptr = cast<ir::DeferencePointer>(ir_object_deference_ptr0);
            if (!ir_object_deference_ptr) continue;

            changed = true;
            auto ir_struct_get_element_ptr = ir::GetStructElementPointer::create(
                ir_object_deference_ptr->pointer(), ir_access_field->field);

            for (auto instruction_with_index_list :
                 clone(ir_access_field->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                size_t op_idx = instruction_with_index_list.operand_index;

                auto ir_deference_pointer = ir::DeferencePointer::create(ir_struct_get_element_ptr);
                auto iter_inst = std::find(RANGE(ir_inst->parent_block->values), ir_inst);
                ir_inst->parent_block->insert(iter_inst, ir_deference_pointer);
                ir_inst->operand(op_idx, ir_deference_pointer);
            }

            utility::replaceInBlock(ir_access_field, ir_struct_get_element_ptr);
            ir_access_field->finalize();
        }
    }

    return changed;
}

inline bool convertIndexArrayToGetArrayElementPointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;

    for (auto ir_function : ir_module->functions) {
        //
        auto ir_index_arrays = utility::getValuesInFunction<ir::IndexArray>(ir_function);

        for (auto ir_index_array : ir_index_arrays) {
            changed = true;

            auto ir_object_deference_ptr = cast<ir::DeferencePointer>(ir_index_array->object());
            if (!ir_object_deference_ptr) continue;

            auto ir_array_get_element_ptr = ir::GetArrayElementPointer::create(
                ir_object_deference_ptr->pointer(), ir_index_array->index());

            for (auto instruction_with_index_list :
                 clone(ir_index_array->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                size_t op_idx = instruction_with_index_list.operand_index;

                auto ir_deference_pointer = ir::DeferencePointer::create(ir_array_get_element_ptr);
                auto iter_inst = std::find(RANGE(ir_inst->parent_block->values), ir_inst);
                ir_inst->parent_block->insert(iter_inst, ir_deference_pointer);
                ir_inst->operand(op_idx, ir_deference_pointer);
            }

            utility::replaceInBlock(ir_index_array, ir_array_get_element_ptr);
            ir_index_array->finalize();
        }
    }

    return changed;
}

inline bool convertIndexPointerToGetPointerElementPointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;

    for (auto ir_function : ir_module->functions) {
        //
        auto ir_index_pointers = utility::getValuesInFunction<ir::IndexPointer>(ir_function);

        for (auto ir_index_pointer : ir_index_pointers) {
            changed = true;

            auto ir_object_deference_ptr = cast<ir::DeferencePointer>(ir_index_pointer->object());
            if (!ir_object_deference_ptr) continue;

            auto ir_pointer_get_element_ptr = ir::GetPointerElementPointer::create(
                ir_object_deference_ptr->pointer(), ir_index_pointer->index());

            for (auto instruction_with_index_list :
                 clone(ir_index_pointer->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                size_t op_idx = instruction_with_index_list.operand_index;

                auto ir_deference_pointer =
                    ir::DeferencePointer::create(ir_pointer_get_element_ptr);
                auto iter_inst = std::find(RANGE(ir_inst->parent_block->values), ir_inst);
                ir_inst->parent_block->insert(iter_inst, ir_deference_pointer);
                ir_inst->operand(op_idx, ir_deference_pointer);
            }

            utility::replaceInBlock(ir_index_pointer, ir_pointer_get_element_ptr);
            ir_index_pointer->finalize();
        }
    }

    return changed;
}

inline bool convertGetAddressOfVaraibleLikedToPointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;

    for (auto ir_function : ir_module->functions) {
        //
        auto ir_get_addresses =
            utility::getValuesInFunction<ir::GetAddressOfVariableLiked>(ir_function);

        for (auto ir_get_address : ir_get_addresses) {
            auto ir_deference_pointer = cast<ir::DeferencePointer>(ir_get_address->variable());
            if (!ir_deference_pointer) continue;

            changed = true;
            for (auto instruction_with_index_list :
                 clone(ir_get_address->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                size_t op_idx = instruction_with_index_list.operand_index;

                ir_deference_pointer->pointer();
                PRAJNA_ASSERT(ir_deference_pointer->pointer());
                ir_inst->operand(op_idx, ir_deference_pointer->pointer());
            }

            utility::removeFromParent(ir_get_address);
            ir_get_address->finalize();
        }
    }

    return changed;
}

inline bool convertDeferencePointerToStoreAndLoadPointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;

    for (auto ir_function : ir_module->functions) {
        //
        auto ir_deference_pointers =
            utility::getValuesInFunction<ir::DeferencePointer>(ir_function);

        for (auto ir_deference_pointer : ir_deference_pointers) {
            changed = true;
            auto ir_pointer = ir_deference_pointer->pointer();
            auto iter_deference_pointer =
                ir_deference_pointer->parent_block->find(ir_deference_pointer);
            for (auto instruction_with_index :
                 clone(ir_deference_pointer->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index.instruction;
                size_t op_idx = instruction_with_index.operand_index;

                if (is<ir::WriteVariableLiked>(ir_inst) && op_idx == 1) {
                    auto ir_write_variable_liked = cast<ir::WriteVariableLiked>(ir_inst);
                    auto ir_store =
                        ir::StorePointer::create(ir_write_variable_liked->value(), ir_pointer);
                    utility::replaceInBlock(ir_write_variable_liked, ir_store);
                } else {
                    auto ir_load = ir::LoadPointer::create(ir_pointer);
                    ir_deference_pointer->parent_block->insert(iter_deference_pointer, ir_load);
                    ir_inst->operand(op_idx, ir_load);
                }
            }
            ir_deference_pointer->parent_block->erase(iter_deference_pointer);
            ir_deference_pointer->finalize();
        }
    }

    return changed;
}

}  // namespace prajna::transform
