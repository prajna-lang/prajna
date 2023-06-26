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
        PRAJNA_ASSERT(ir_value->parent_block == ir_block);
        PRAJNA_ASSERT(defined_values.count(ir_value) == 0);
        defined_values.insert(ir_value);

        if (auto ir_block = cast<ir::Block>(ir_value)) {
            VerifyBlockImpl(ir_block, defined_values);
            continue;
        }

        if (auto ir_if = cast<ir::If>(ir_value)) {
            VerifyBlockImpl(ir_if->trueBlock(), defined_values);
            VerifyBlockImpl(ir_if->falseBlock(), defined_values);
            continue;
        }

        if (auto ir_while = cast<ir::While>(ir_value)) {
            VerifyBlockImpl(ir_while->loopBlock(), defined_values);
            continue;
        }

        if (auto ir_for = cast<ir::For>(ir_value)) {
            VerifyBlockImpl(ir_for->loopBlock(), defined_values);
            continue;
        }

        if (auto ir_instruction = cast<ir::Instruction>(ir_value)) {
            for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
                auto ir_operand = ir_instruction->operand(i);
                PRAJNA_ASSERT(ir_operand);

                if (is<ir::Function>(ir_operand) or is<ir::GlobalVariable>(ir_operand) or
                    is<ir::GlobalAlloca>(ir_operand) or is<ir::Parameter>(ir_operand)) {
                    continue;
                }

                // block存在跳转的情况
                if (!is<ir::Block>(ir_operand)) {
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
        auto ir_instructions = utility::getValuesInFunction<ir::Instruction>(ir_function);
        for (auto ir_instruction : ir_instructions) {
            if (is<ir::For>(ir_instruction)) {
                continue;
            }

            for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
                auto ir_operand = ir_instruction->operand(i);

                PRAJNA_ASSERT(ir_operand);

                if (ir_operand) {
                    if (is<ir::Function>(ir_operand) or is<ir::GlobalVariable>(ir_operand) or
                        is<ir::GlobalAlloca>(ir_operand) or is<ir::Parameter>(ir_operand)) {
                        continue;
                    }

                    // 如果是While For等Block则直接放回, 其无法溯源到跟函数
                    if (ir_operand->getRootBlock()->instruction_with_index_list.size() > 0) {
                        continue;
                    }

                    PRAJNA_ASSERT(std::find(RANGE(ir_operand->parent_block->values), ir_operand) !=
                                  ir_operand->parent_block->values.end());

                    auto ir_function_tmp = ir_operand->getParentFunction();
                    PRAJNA_ASSERT(ir_function_tmp == ir_function);
                }
            }
        }
    }

    auto ir_instructions = utility::getValuesInModule<ir::Call>(ir_module);
    for (auto ir_instruction : ir_instructions) {
        for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
            auto ir_operand = ir_instruction->operand(i);
            if (is<ir::Function>(ir_operand)) {
                for (auto [ir_inst_cur, op_idx] : clone(ir_operand->instruction_with_index_list)) {
                    // TODO,  目前extractGpu无法通过, 后续修复
                    // PRAJNA_ASSERT(ir_inst_cur->getParentFunction());
                }
            }
        }
    }
    return true;
}

}  // namespace prajna::transform
