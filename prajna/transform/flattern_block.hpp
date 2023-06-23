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

inline bool flatternBlockImpl(std::shared_ptr<ir::Block> ir_block) {
    bool changed = false;
    for (auto iter = ir_block->values.begin(); iter != ir_block->values.end();) {
        if (auto ir_cur_block = cast<ir::Block>(*iter)) {
            // Label也是Block的一种, 但其不能展开
            if (is<ir::Label>(ir_cur_block)) {
                ++iter;
                continue;
            }

            flatternBlockImpl(ir_cur_block);
            for (auto ir_value : ir_cur_block->values) {
                ir_block->insert(iter, ir_value);
            }
            iter = ir_block->values.erase(iter);
            ir_cur_block->finalize();
            changed = true;
            continue;
        }
        if (auto ir_if = cast<ir::If>(*iter)) {
            flatternBlockImpl(ir_if->trueBlock());
            flatternBlockImpl(ir_if->falseBlock());

            auto ir_true_label = ir::Label::create();
            auto ir_false_label = ir::Label::create();
            ir_if->trueBlock()->pushFront(ir_true_label);
            ir_if->falseBlock()->pushFront(ir_false_label);
            auto ir_condition_branch =
                ir::ConditionBranch::create(ir_if->condition(), ir_true_label, ir_false_label);

            auto ir_end_merge_label = ir::Label::create();
            auto ir_true_branch = ir::JumpBranch::create(ir_end_merge_label);
            ir_if->trueBlock()->PushBack(ir_true_branch);
            auto ir_false_branch = ir::JumpBranch::create(ir_end_merge_label);
            ir_if->falseBlock()->PushBack(ir_false_branch);

            ir_block->insert(iter, ir_condition_branch);
            for (auto e : ir_if->trueBlock()->values) {
                ir_block->insert(iter, e);
            }
            for (auto e : ir_if->falseBlock()->values) {
                ir_block->insert(iter, e);
            }
            ir_block->insert(iter, ir_end_merge_label);

            iter = ir_block->values.erase(iter);
            ir_if->finalize();

            changed = true;
            continue;
        }
        if (auto ir_while = cast<ir::While>(*iter)) {
            flatternBlockImpl(ir_while->conditionBlock());
            flatternBlockImpl(ir_while->loopBlock());

            auto ir_label_loop = ir::Label::create();
            ir_while->loopBlock()->pushFront(ir_label_loop);
            auto ir_label_after_loop = ir::Label::create();
            auto ir_condition_branch = ir::ConditionBranch::create(
                ir_while->condition(), ir_label_loop, ir_label_after_loop);
            ir_while->conditionBlock()->PushBack(ir_condition_branch);

            auto ir_label_condition_entry = ir::Label::create();
            auto ir_jump_branch = ir::JumpBranch::create(ir_label_condition_entry);
            ir_while->loopBlock()->PushBack(ir_jump_branch);

            //在while开始的地方, 需要jump到conditionBlock(),
            auto ir_concat_branch = ir::JumpBranch::create(ir_label_condition_entry);
            ir_block->insert(iter, ir_concat_branch);
            ir_block->insert(iter, ir_label_condition_entry);

            for (auto e : ir_while->conditionBlock()->values) {
                ir_block->insert(iter, e);
            }
            for (auto e : ir_while->loopBlock()->values) {
                ir_block->insert(iter, e);
            }
            ir_block->insert(iter, ir_label_after_loop);

            auto ir_builder = lowering::IrBuilder::create();
            ir_builder->pushBlock(ir_block);

            for (auto [ir_instruction, op_idx] : clone(ir_while->instruction_with_index_list)) {
                if (auto ir_break = cast<ir::Break>(ir_instruction)) {
                    ir_builder->inserter_iterator = ir_break->GetBlockIterator();
                    ir_builder->create<ir::JumpBranch>(ir_label_after_loop);
                    utility::removeFromParent(ir_break);
                    ir_break->finalize();
                    continue;
                }

                if (auto ir_continue = cast<ir::Continue>(ir_instruction)) {
                    ir_builder->inserter_iterator = ir_continue->GetBlockIterator();
                    ir_builder->create<ir::JumpBranch>(ir_label_condition_entry);
                    utility::removeFromParent(ir_continue);
                    ir_continue->finalize();
                    continue;
                }

                PRAJNA_UNREACHABLE;
            }

            iter = ir_block->values.erase(iter);
            ir_while->finalize();

            changed = true;
            continue;
        }
        if (auto ir_for = cast<ir::For>(*iter)) {
            // 即使标注了gpu, 其内部也应该展开以便简化后续的分析流程
            flatternBlockImpl(ir_for->loopBlock());

            // 标注gpu的则在后面抽离,
            if (ir_for->annotation_dict.count("gpu")) {
                ++iter;
                continue;
            }

            auto ir_builder = lowering::IrBuilder::create();
            ir_builder->pushBlock(ir_block);
            ir_builder->inserter_iterator = iter;

            // 创建用于记录迭代的变量,
            auto ir_first_sub_one = ir_builder->callBinaryOperator(ir_for->first(), "-",
                                                                   ir_builder->getIndexConstant(1));
            auto ir_index_count = ir_builder->cloneValue(ir_first_sub_one);

            auto ir_label_condition_entry = ir::Label::create();
            auto ir_concat_jump = ir_builder->create<ir::JumpBranch>(ir_label_condition_entry);
            ir_block->insert(iter, ir_label_condition_entry);
            auto ir_label_loop = ir::Label::create();
            // insertCallMemmberFunction会插入ir_condition
            auto ir_index_count_add_one = ir_builder->callBinaryOperator(
                ir_index_count, "+", {ir_builder->getIndexConstant(1)});
            ir_builder->create<ir::WriteVariableLiked>(ir_index_count_add_one, ir_index_count);
            auto ir_condition =
                ir_builder->callBinaryOperator(ir_index_count, "<", {ir_for->last()});
            auto ir_label_after_loop = ir::Label::create();
            auto ir_condition_branch = ir_builder->create<ir::ConditionBranch>(
                ir_condition, ir_label_loop, ir_label_after_loop);

            ir_builder->pushBlock(ir_for->loopBlock());
            // 给ir_for->index()赋值
            ir_builder->inserter_iterator = ir_builder->currentBlock()->values.begin();
            ir_builder->create<ir::WriteVariableLiked>(ir_index_count, ir_for->index());
            // 需要在后面执行, 插入到最前面去
            ir_for->loopBlock()->pushFront(ir_label_loop);
            ir_builder->popBlock();

            auto ir_jump_branch = ir::JumpBranch::create(ir_label_condition_entry);
            ir_for->loopBlock()->PushBack(ir_jump_branch);
            for (auto e : ir_for->loopBlock()->values) {
                ir_block->insert(iter, e);
            }
            ir_block->insert(iter, ir_label_after_loop);

            for (auto [ir_instruction, op_idx] : clone(ir_for->instruction_with_index_list)) {
                if (auto ir_break = cast<ir::Break>(ir_instruction)) {
                    ir_builder->inserter_iterator = ir_break->GetBlockIterator();
                    ir_builder->create<ir::JumpBranch>(ir_label_after_loop);
                    utility::removeFromParent(ir_break);
                    ir_break->finalize();
                    continue;
                }

                if (auto ir_continue = cast<ir::Continue>(ir_instruction)) {
                    ir_builder->inserter_iterator = ir_continue->GetBlockIterator();
                    ir_builder->create<ir::JumpBranch>(ir_label_condition_entry);
                    utility::removeFromParent(ir_continue);
                    ir_continue->finalize();
                    continue;
                }

                PRAJNA_UNREACHABLE;
            }

            iter = ir_block->values.erase(iter);
            ir_for->finalize();

            changed = true;
            continue;
        }

        ++iter;
    }

    return changed;
}

inline std::list<std::shared_ptr<ir::Block>> splitBlock(std::shared_ptr<ir::Block> ir_top_block) {
    std::list<std::shared_ptr<ir::Block>> blocks;
    blocks.push_back(ir::Block::create());  // @note 函数开头不能插入ir::Label否则和这里矛盾
    for (auto ir_value : ir_top_block->values) {
        PRAJNA_ASSERT(!is<ir::Block>(ir_value) || is<ir::Label>(ir_value));
        if (auto ir_label = cast<ir::Label>(ir_value)) {
            blocks.push_back(ir::Block::create());

            // ir_label作为操作时修改时(移除或添加), 都会改变instructions_with_index的值,
            // 故应该先拷贝一份
            for (auto inst_with_idx : clone(ir_label->instruction_with_index_list)) {
                inst_with_idx.instruction->operand(inst_with_idx.operand_index, blocks.back());
            }
        } else {
            blocks.back()->PushBack(ir_value);
        }
    }

    return blocks;
}

}  // namespace

inline std::shared_ptr<ir::Module> flatternBlock(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        // @note 后面可能需要重构, 目前假设只有一个block时才需要展开
        if (ir_function->blocks.size() != 1) continue;

        auto ir_top_block = ir_function->blocks.front();
        flatternBlockImpl(ir_top_block);
        // 分离成标准ssa的block形式
        ir_function->blocks = splitBlock(ir_top_block);
        for (auto ir_block : ir_function->blocks) {
            ir_block->parent_function = ir_function;
            ir_block->parent_block = nullptr;
        }
    }

    for (auto [ir_target, ir_sub_module] : ir_module->modules) {
        if (not ir_sub_module) continue;

        flatternBlock(ir_sub_module);
    }

    return ir_module;
}

}  // namespace prajna::transform
