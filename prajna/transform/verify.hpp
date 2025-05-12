#pragma once

#include <boost/range/combine.hpp>

#include "prajna/ir/ir.hpp"
#include "prajna/logger.hpp"
#include "prajna/lowering/statement_lowering_visitor.hpp"
#include "prajna/parser/parse.h"
#include "prajna/transform/transform_pass.hpp"
#include "prajna/transform/transform_to_ssa_pass.hpp"
#include "prajna/transform/utility.hpp"

namespace prajna::transform {

inline void VerifyBlockImpl(std::shared_ptr<ir::Block> ir_block,
                            std::set<std::shared_ptr<ir::Value>>& defined_values) {
    for (auto ir_value : ir_block->values) {
        auto parent = ir_value->parent_block.lock();
        PRAJNA_ASSERT(parent == ir_block);
        PRAJNA_ASSERT(defined_values.count(ir_value) == 0);
        defined_values.insert(ir_value);

        if (auto ir_block = Cast<ir::Block>(ir_value)) {
            VerifyBlockImpl(ir_block, defined_values);
            continue;
        }

        if (auto ir_if = Cast<ir::If>(ir_value)) {
            VerifyBlockImpl(ir_if->TrueBlock(), defined_values);
            VerifyBlockImpl(ir_if->FalseBlock(), defined_values);
            continue;
        }

        if (auto ir_while = Cast<ir::While>(ir_value)) {
            VerifyBlockImpl(ir_while->ConditionBlock(), defined_values);
            VerifyBlockImpl(ir_while->LoopBlock(), defined_values);
            continue;
        }

        if (auto ir_for = Cast<ir::For>(ir_value)) {
            VerifyBlockImpl(ir_for->LoopBlock(), defined_values);
            continue;
        }

        if (auto ir_instruction = Cast<ir::Instruction>(ir_value)) {
            for (int64_t i = 0; i < ir_instruction->OperandSize(); ++i) {
                auto ir_operand = ir_instruction->GetOperand(i);
                PRAJNA_ASSERT(ir_operand);

                if (Is<ir::Function>(ir_operand) || Is<ir::GlobalVariable>(ir_operand) ||
                    Is<ir::GlobalAlloca>(ir_operand) || Is<ir::Parameter>(ir_operand)) {
                    continue;
                }

                // block存在跳转的情况
                if (!Is<ir::Block>(ir_operand)) {
                    PRAJNA_ASSERT(defined_values.count(ir_operand) != 0);
                }
            }
        }
    }
}

inline void VerifyFunction(std::shared_ptr<ir::Function> ir_function) {
    std::set<std::shared_ptr<ir::Value>> defined_values;
    for (auto ir_block : ir_function->blocks) {
        VerifyBlockImpl(ir_block, defined_values);
    }
}

inline bool VerifyModule(std::shared_ptr<ir::Module> ir_module) {
    std::set<std::string> global_names;
    for (auto ir_function : ir_module->functions) {
        PRAJNA_ASSERT(global_names.count(ir_function->fullname) == 0);
        global_names.insert(ir_function->fullname);
        VerifyFunction(ir_function);
    }

    for (auto ir_function : ir_module->functions) {
        auto ir_instructions = utility::GetValuesInFunction<ir::Instruction>(ir_function);
        for (auto ir_instruction : ir_instructions) {
            if (Is<ir::For>(ir_instruction)) {
                continue;
            }

            for (int64_t i = 0; i < ir_instruction->OperandSize(); ++i) {
                auto ir_operand = ir_instruction->GetOperand(i);

                PRAJNA_ASSERT(ir_operand);

                if (ir_operand) {
                    if (ir_operand->is_global || Is<ir::Parameter>(ir_operand)) continue;

                    // 如果是While For等Block则直接放回, 其无法溯源到跟函数
                    if (ir_operand->GetRootBlock()->instruction_with_index_list.size() > 0) {
                        continue;
                    }
                    auto parent = Lock(ir_operand->parent_block);
                    PRAJNA_ASSERT(std::find(RANGE(parent->values), ir_operand) !=
                                  parent->values.end());

                    auto ir_function_tmp = ir_operand->GetParentFunction();
                    PRAJNA_ASSERT(ir_function_tmp == ir_function);
                }
            }
        }
    }

    return true;
}

}  // namespace prajna::transform
