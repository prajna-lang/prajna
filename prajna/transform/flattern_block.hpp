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

namespace {

inline bool FlatternBlockImpl(std::shared_ptr<ir::Block> ir_block) {
    bool changed = false;
    for (auto iter = ir_block->values.begin(); iter != ir_block->values.end();) {
        if (auto ir_cur_block = cast<ir::Block>(*iter)) {
            // Label也是Block的一种, 但其不能展开
            if (Is<ir::Label>(ir_cur_block)) {
                ++iter;
                continue;
            }

            FlatternBlockImpl(ir_cur_block);
            for (auto ir_value : ir_cur_block->values) {
                ir_block->insert(iter, ir_value);
            }
            iter = ir_block->values.erase(iter);
            ir_cur_block->Finalize();
            changed = true;
            continue;
        }
        if (auto ir_if = cast<ir::If>(*iter)) {
            FlatternBlockImpl(ir_if->TrueBlock());
            FlatternBlockImpl(ir_if->FalseBlock());

            auto ir_true_label = ir::Label::Create();
            auto ir_false_label = ir::Label::Create();
            ir_if->TrueBlock()->pushFront(ir_true_label);
            ir_if->FalseBlock()->pushFront(ir_false_label);
            auto ir_condition_branch =
                ir::ConditionBranch::Create(ir_if->condition(), ir_true_label, ir_false_label);

            auto ir_end_merge_label = ir::Label::Create();
            auto ir_true_branch = ir::JumpBranch::Create(ir_end_merge_label);
            ir_if->TrueBlock()->PushBack(ir_true_branch);
            auto ir_false_branch = ir::JumpBranch::Create(ir_end_merge_label);
            ir_if->FalseBlock()->PushBack(ir_false_branch);

            ir_block->insert(iter, ir_condition_branch);
            for (auto e : ir_if->TrueBlock()->values) {
                ir_block->insert(iter, e);
            }
            for (auto e : ir_if->FalseBlock()->values) {
                ir_block->insert(iter, e);
            }
            ir_block->insert(iter, ir_end_merge_label);

            iter = ir_block->values.erase(iter);
            ir_if->Finalize();

            changed = true;
            continue;
        }
        if (auto ir_while = cast<ir::While>(*iter)) {
            FlatternBlockImpl(ir_while->ConditionBlock());
            FlatternBlockImpl(ir_while->LoopBlock());

            auto ir_label_loop = ir::Label::Create();
            ir_while->LoopBlock()->pushFront(ir_label_loop);
            auto ir_label_after_loop = ir::Label::Create();
            auto ir_condition_branch = ir::ConditionBranch::Create(
                ir_while->condition(), ir_label_loop, ir_label_after_loop);
            ir_while->ConditionBlock()->PushBack(ir_condition_branch);

            auto ir_label_condition_entry = ir::Label::Create();
            auto ir_jump_branch = ir::JumpBranch::Create(ir_label_condition_entry);
            ir_while->LoopBlock()->PushBack(ir_jump_branch);

            //在while开始的地方, 需要jump到conditionBlock(),
            auto ir_concat_branch = ir::JumpBranch::Create(ir_label_condition_entry);
            ir_block->insert(iter, ir_concat_branch);
            ir_block->insert(iter, ir_label_condition_entry);

            for (auto e : ir_while->ConditionBlock()->values) {
                ir_block->insert(iter, e);
            }
            for (auto e : ir_while->LoopBlock()->values) {
                ir_block->insert(iter, e);
            }
            ir_block->insert(iter, ir_label_after_loop);

            auto ir_builder = lowering::IrBuilder::Create();
            ir_builder->PushBlock(ir_block);

            for (auto [ir_instruction, op_idx] : Clone(ir_while->instruction_with_index_list)) {
                if (auto ir_break = cast<ir::Break>(ir_instruction)) {
                    ir_builder->inserter_iterator = ir_break->GetBlockIterator();
                    ir_builder->Create<ir::JumpBranch>(ir_label_after_loop);
                    utility::RemoveFromParent(ir_break);
                    ir_break->Finalize();
                    continue;
                }

                if (auto ir_continue = cast<ir::Continue>(ir_instruction)) {
                    ir_builder->inserter_iterator = ir_continue->GetBlockIterator();
                    ir_builder->Create<ir::JumpBranch>(ir_label_condition_entry);
                    utility::RemoveFromParent(ir_continue);
                    ir_continue->Finalize();
                    continue;
                }

                PRAJNA_UNREACHABLE;
            }

            iter = ir_block->values.erase(iter);
            ir_while->Finalize();

            changed = true;
            continue;
        }
        if (auto ir_for = cast<ir::For>(*iter)) {
            // 即使标注了gpu, 其内部也应该展开以便简化后续的分析流程
            FlatternBlockImpl(ir_for->LoopBlock());

            // 标注gpu的则在后面抽离,
            if (ir_for->annotation_dict.count("gpu")) {
                ++iter;
                continue;
            }

            auto ir_builder = lowering::IrBuilder::Create();
            ir_builder->PushBlock(ir_block);
            ir_builder->inserter_iterator = iter;

            // 创建用于记录迭代的变量,
            auto ir_first_sub_one = ir_builder->CallBinaryOperator(ir_for->first(), "-",
                                                                   ir_builder->GetIndexConstant(1));
            auto ir_index_count = ir_builder->CloneValue(ir_first_sub_one);

            auto ir_label_condition_entry = ir::Label::Create();
            auto ir_concat_jump = ir_builder->Create<ir::JumpBranch>(ir_label_condition_entry);
            ir_block->insert(iter, ir_label_condition_entry);
            auto ir_label_loop = ir::Label::Create();
            // insertCallMemmberFunction会插入ir_condition
            auto ir_index_count_add_one = ir_builder->CallBinaryOperator(
                ir_index_count, "+", {ir_builder->GetIndexConstant(1)});
            ir_builder->Create<ir::WriteVariableLiked>(ir_index_count_add_one, ir_index_count);
            auto ir_condition =
                ir_builder->CallBinaryOperator(ir_index_count, "<", {ir_for->last()});
            auto ir_label_after_loop = ir::Label::Create();
            auto ir_condition_branch = ir_builder->Create<ir::ConditionBranch>(
                ir_condition, ir_label_loop, ir_label_after_loop);

            ir_builder->PushBlock(ir_for->LoopBlock());
            // 给ir_for->index()赋值
            ir_builder->inserter_iterator = ir_builder->currentBlock()->values.begin();
            ir_builder->Create<ir::WriteVariableLiked>(ir_index_count, ir_for->index());
            // 需要在后面执行, 插入到最前面去
            ir_for->LoopBlock()->pushFront(ir_label_loop);
            ir_builder->PopBlock();

            auto ir_jump_branch = ir::JumpBranch::Create(ir_label_condition_entry);
            ir_for->LoopBlock()->PushBack(ir_jump_branch);
            for (auto e : ir_for->LoopBlock()->values) {
                ir_block->insert(iter, e);
            }
            ir_block->insert(iter, ir_label_after_loop);

            for (auto [ir_instruction, op_idx] : Clone(ir_for->instruction_with_index_list)) {
                if (auto ir_break = cast<ir::Break>(ir_instruction)) {
                    ir_builder->inserter_iterator = ir_break->GetBlockIterator();
                    ir_builder->Create<ir::JumpBranch>(ir_label_after_loop);
                    utility::RemoveFromParent(ir_break);
                    ir_break->Finalize();
                    continue;
                }

                if (auto ir_continue = cast<ir::Continue>(ir_instruction)) {
                    ir_builder->inserter_iterator = ir_continue->GetBlockIterator();
                    ir_builder->Create<ir::JumpBranch>(ir_label_condition_entry);
                    utility::RemoveFromParent(ir_continue);
                    ir_continue->Finalize();
                    continue;
                }

                PRAJNA_UNREACHABLE;
            }

            iter = ir_block->values.erase(iter);
            ir_for->Finalize();

            changed = true;
            continue;
        }

        ++iter;
    }

    return changed;
}

inline std::list<std::shared_ptr<ir::Block>> splitBlock(std::shared_ptr<ir::Block> ir_top_block) {
    std::list<std::shared_ptr<ir::Block>> blocks;
    blocks.push_back(ir::Block::Create());  // @note 函数开头不能插入ir::Label否则和这里矛盾
    for (auto ir_value : ir_top_block->values) {
        PRAJNA_ASSERT(!Is<ir::Block>(ir_value) || Is<ir::Label>(ir_value));
        if (auto ir_label = cast<ir::Label>(ir_value)) {
            blocks.push_back(ir::Block::Create());

            // ir_label作为操作时修改时(移除或添加), 都会改变instructions_with_index的值,
            // 故应该先拷贝一份
            for (auto inst_with_idx : Clone(ir_label->instruction_with_index_list)) {
                inst_with_idx.instruction->operand(inst_with_idx.operand_index, blocks.back());
            }
        } else {
            blocks.back()->PushBack(ir_value);
        }
    }

    return blocks;
}

}  // namespace

inline bool FlatternBlock(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        std::list<std::shared_ptr<ir::Block>> blocks;
        for (auto ir_block : ir_function->blocks) {
            flatternBlockImpl(ir_block);
            blocks.merge(splitBlock(ir_block));
        }

        ir_function->blocks = blocks;

        for (auto ir_block : ir_function->blocks) {
            ir_block->parent_function = ir_function;
            ir_block->parent_block = nullptr;
        }
    }

    return false;
}

}  // namespace prajna::transform
