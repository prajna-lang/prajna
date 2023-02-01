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
    auto ir_blocks = utility::getValuesInModule<ir::Block>(ir_module);
    for (auto ir_block : ir_blocks) {
        for (auto ir_value : ir_block->values) {
            PRAJNA_ASSERT(ir_value->parent_block == ir_block);
        }
    }

    for (auto ir_function : ir_module->functions) {
        auto ir_instructions = utility::getValuesInFunction<ir::Instruction>(ir_function);
        for (auto ir_instruction : ir_instructions) {
            for (size_t i = 0; i < ir_instruction->operandSize(); ++i) {
                auto ir_operand = ir_instruction->operand(i);

                if (!(is<ir::Function>(ir_operand) or is<ir::GlobalVariable>(ir_operand) or
                      is<ir::GlobalAlloca>(ir_operand) or is<ir::Argument>(ir_operand))) {
                    // 如果是While For等Block则直接放回, 其无法溯源到跟函数
                    if (ir_operand->getRootBlock()->instruction_with_index_list.size() > 0) {
                        continue;
                    }

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
                    PRAJNA_ASSERT(ir_inst_cur->getParentFunction());
                }
            }
        }
    }
    return true;
}

}  // namespace prajna::transform
