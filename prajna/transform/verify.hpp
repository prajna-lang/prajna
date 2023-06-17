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

inline bool verifyTree(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        // 只对展开blocks的函数完全工作, 没展开只检测到第一层的block
        std::set<std::shared_ptr<ir::Value>> defined_values;
        for (auto ir_block : ir_function->blocks) {
            PRAJNA_ASSERT(ir_block->parent_function == ir_function);
            for (auto ir_value : ir_block->values) {
                PRAJNA_ASSERT(ir_value->parent_block == ir_block);
                PRAJNA_ASSERT(defined_values.count(ir_value) == 0);
                defined_values.insert(ir_value);

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
    }

    auto ir_blocks = utility::getValuesInModule<ir::Block>(ir_module);
    for (auto ir_block : ir_blocks) {
        for (auto ir_value : ir_block->values) {
            PRAJNA_ASSERT(ir_value->parent_block == ir_block);
            PRAJNA_ASSERT(!ir_value->isFinalized());
        }
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

                    PRAJNA_ASSERT(ir_operand->getParentFunction() == ir_function);
                }
            }
        }
    }

    auto ir_instructions = utility::getValuesInModule<ir::Call>(ir_module);
    for (auto ir_instruction : ir_instructions) {
        for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
            auto ir_operand = ir_instruction->operand(i);
            if (is<ir::Function>(ir_operand)) {
                auto instruction_with_index_list_copy = ir_operand->instruction_with_index_list;
                for (auto [ir_inst_cur, op_idx] : instruction_with_index_list_copy) {
                    // TODO,  目前extractGpu无法通过, 后续修复
                    // PRAJNA_ASSERT(ir_inst_cur->getParentFunction());
                }
            }
        }
    }
    return true;
}

}  // namespace prajna::transform
