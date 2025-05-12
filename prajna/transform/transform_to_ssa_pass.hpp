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

inline bool InsertValueToBlock(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;
    for (auto ir_function : ir_module->functions) {
        std::set<std::shared_ptr<ir::Value>> ir_values;
        for (auto ir_block : ir_function->blocks) {
            for (auto iter_ir_value = ir_block->values.begin();
                 iter_ir_value != ir_block->values.end(); ++iter_ir_value) {
                auto ir_value = *iter_ir_value;
                ir_values.insert(ir_value);

                if (auto ir_instruction = Cast<ir::Instruction>(ir_value)) {
                    for (auto i = 0; i < ir_instruction->OperandSize(); ++i) {
                        auto ir_operand = ir_instruction->GetOperand(i);
                        if (ir_values.count(ir_operand) == 0) {
                            // 模板实例化时会产生非Block的Constant,并且会被使用
                            if (Is<ir::ConstantInt>(ir_operand)) {
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

inline bool ConvertVariableToDeferencePointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;

    for (auto ir_function : ir_module->functions) {
        //
        auto ir_variables = utility::GetValuesInFunction<ir::LocalVariable>(ir_function);

        for (auto ir_variable : ir_variables) {
            //改变它自己
            changed = true;
            auto ir_constant_1 = ir::ConstantInt::Create(ir::IntType::Create(64, false), 1);
            std::shared_ptr<ir::Value> ir_alloca =
                ir::Alloca::Create(ir_variable->type, ir_constant_1);
            ir_alloca->name = ir_variable->name;
            ir_alloca->fullname = ir_alloca->name;

            // @note ir::Alloca不应该出现在循环体内部, 故直接再将其放在函数的第一个块
            ir_function->blocks.front()->PushFront(ir_alloca);
            ir_function->blocks.front()->PushFront(ir_constant_1);
            Lock(ir_variable->parent_block)->remove(ir_variable);

            // 改变使用它的
            for (auto instruction_with_index_list :
                 Clone(ir_variable->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                int64_t op_idx = instruction_with_index_list.operand_index;

                auto ir_deference_pointer = ir::DeferencePointer::Create(ir_alloca);
                auto ir_block = Lock(ir_inst->parent_block);
                auto iter = std::find(ir_block->values.begin(), ir_block->values.end(), ir_inst);
                ir_block->insert(iter, ir_deference_pointer);
                ir_inst->SetOperand(op_idx, ir_deference_pointer);
            }

            ir_variable->Finalize();
        }
    }

    return changed;
}

inline bool ConvertAccessFieldToGetStructElementPointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;
    for (auto ir_function : ir_module->functions) {
        auto ir_access_fields = utility::GetValuesInFunction<ir::AccessField>(ir_function);

        for (auto ir_access_field : ir_access_fields) {
            auto ir_object_deference_ptr0 = (ir_access_field->object());
            auto ir_object_deference_ptr = Cast<ir::DeferencePointer>(ir_object_deference_ptr0);
            if (!ir_object_deference_ptr) continue;

            changed = true;
            auto ir_struct_get_element_ptr = ir::GetStructElementPointer::Create(
                ir_object_deference_ptr->Pointer(), ir_access_field->field);

            for (auto instruction_with_index_list :
                 Clone(ir_access_field->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                int64_t op_idx = instruction_with_index_list.operand_index;

                auto ir_deference_pointer = ir::DeferencePointer::Create(ir_struct_get_element_ptr);
                auto parent = Lock(ir_inst->parent_block);
                auto iter_inst = std::find(RANGE(parent->values), ir_inst);
                parent->insert(iter_inst, ir_deference_pointer);
                ir_inst->SetOperand(op_idx, ir_deference_pointer);
            }

            utility::ReplaceInBlock(ir_access_field, ir_struct_get_element_ptr);
            ir_access_field->Finalize();
        }
    }

    return changed;
}

inline bool ConvertIndexArrayToGetArrayElementPointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;

    for (auto ir_function : ir_module->functions) {
        //
        auto ir_index_arrays = utility::GetValuesInFunction<ir::IndexArray>(ir_function);

        for (auto ir_index_array : ir_index_arrays) {
            changed = true;

            auto ir_object_deference_ptr = Cast<ir::DeferencePointer>(ir_index_array->object());
            if (!ir_object_deference_ptr) continue;

            auto ir_array_get_element_ptr = ir::GetArrayElementPointer::Create(
                ir_object_deference_ptr->Pointer(), ir_index_array->IndexVariable());

            for (auto instruction_with_index_list :
                 Clone(ir_index_array->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                int64_t op_idx = instruction_with_index_list.operand_index;

                auto ir_deference_pointer = ir::DeferencePointer::Create(ir_array_get_element_ptr);
                auto parent = Lock(ir_inst->parent_block);
                auto iter_inst = std::find(RANGE(parent->values), ir_inst);
                parent->insert(iter_inst, ir_deference_pointer);
                ir_inst->SetOperand(op_idx, ir_deference_pointer);
            }

            utility::ReplaceInBlock(ir_index_array, ir_array_get_element_ptr);
            ir_index_array->Finalize();
        }
    }

    return changed;
}

inline bool ConvertIndexPointerToGetPointerElementPointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;

    for (auto ir_function : ir_module->functions) {
        //
        auto ir_index_pointers = utility::GetValuesInFunction<ir::IndexPointer>(ir_function);

        for (auto ir_index_pointer : ir_index_pointers) {
            changed = true;

            auto ir_object_deference_ptr = Cast<ir::DeferencePointer>(ir_index_pointer->object());
            if (!ir_object_deference_ptr) continue;

            auto ir_pointer_get_element_ptr = ir::GetPointerElementPointer::Create(
                ir_object_deference_ptr->Pointer(), ir_index_pointer->IndexVariable());

            for (auto instruction_with_index_list :
                 Clone(ir_index_pointer->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                int64_t op_idx = instruction_with_index_list.operand_index;

                auto ir_deference_pointer =
                    ir::DeferencePointer::Create(ir_pointer_get_element_ptr);
                auto parent = Lock(ir_inst->parent_block);
                auto iter_inst = std::find(RANGE(parent->values), ir_inst);
                parent->insert(iter_inst, ir_deference_pointer);
                ir_inst->SetOperand(op_idx, ir_deference_pointer);
            }

            utility::ReplaceInBlock(ir_index_pointer, ir_pointer_get_element_ptr);
            ir_index_pointer->Finalize();
        }
    }

    return changed;
}

inline bool ConvertGetAddressOfVaraibleLikedToPointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;

    for (auto ir_function : ir_module->functions) {
        //
        auto ir_get_addresses =
            utility::GetValuesInFunction<ir::GetAddressOfVariableLiked>(ir_function);

        for (auto ir_get_address : ir_get_addresses) {
            auto ir_deference_pointer = Cast<ir::DeferencePointer>(ir_get_address->variable());
            if (!ir_deference_pointer) continue;

            changed = true;
            for (auto instruction_with_index_list :
                 Clone(ir_get_address->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index_list.instruction;
                int64_t op_idx = instruction_with_index_list.operand_index;

                ir_deference_pointer->Pointer();
                PRAJNA_ASSERT(ir_deference_pointer->Pointer());
                ir_inst->SetOperand(op_idx, ir_deference_pointer->Pointer());
            }

            utility::RemoveFromParent(ir_get_address);
            ir_get_address->Finalize();
        }
    }

    return changed;
}

inline bool ConvertDeferencePointerToStoreAndLoadPointer(std::shared_ptr<ir::Module> ir_module) {
    bool changed = false;

    for (auto ir_function : ir_module->functions) {
        //
        auto ir_deference_pointers =
            utility::GetValuesInFunction<ir::DeferencePointer>(ir_function);

        for (auto ir_deference_pointer : ir_deference_pointers) {
            changed = true;
            auto ir_pointer = ir_deference_pointer->Pointer();
            auto iter_deference_pointer =
                Lock(ir_deference_pointer->parent_block)->find(ir_deference_pointer);
            for (auto instruction_with_index :
                 Clone(ir_deference_pointer->instruction_with_index_list)) {
                auto ir_inst = instruction_with_index.instruction;
                int64_t op_idx = instruction_with_index.operand_index;

                if (Is<ir::WriteVariableLiked>(ir_inst) && op_idx == 1) {
                    auto ir_write_variable_liked = Cast<ir::WriteVariableLiked>(ir_inst);
                    auto ir_store =
                        ir::StorePointer::Create(ir_write_variable_liked->Value(), ir_pointer);
                    utility::ReplaceInBlock(ir_write_variable_liked, ir_store);
                } else {
                    auto ir_load = ir::LoadPointer::Create(ir_pointer);
                    Lock(ir_inst->parent_block)->insert(ir_inst->GetBlockIterator(), ir_load);
                    ir_inst->SetOperand(op_idx, ir_load);
                }
            }
            Lock(ir_deference_pointer->parent_block)->erase(iter_deference_pointer);
            ir_deference_pointer->Finalize();
        }
    }

    return changed;
}

}  // namespace prajna::transform
