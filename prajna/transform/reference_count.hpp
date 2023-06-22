#pragma once

#include <memory>
#include <stack>

#include "prajna/ir/ir.hpp"
#include "prajna/logger.hpp"
#include "prajna/lowering/statement_lowering_visitor.hpp"
#include "prajna/parser/parse.h"
#include "prajna/transform/transform_pass.hpp"
#include "prajna/transform/utility.hpp"

namespace prajna::ir {
class Module;
}

namespace prajna::ast {
class Statements;
}

namespace prajna::transform {

namespace {

const std::string DISABLE_REFERENCE_COUNT = "DisableReferenceCount";

std::shared_ptr<lowering::IrBuilder> makeIRbuilder() {
    auto ir_builder = lowering::IrBuilder::create();
    ir_builder->create_callback = [](std::shared_ptr<ir::Value> ir_value) {
        ir_value->annotation_dict[DISABLE_REFERENCE_COUNT];
    };
    return ir_builder;
}

inline std::shared_ptr<ir::Module> insertLocalVariableRegisterReferenceCount(
    std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_local_variables = utility::getValuesInFunction<ir::LocalVariable>(ir_function);
        ir_local_variables.remove_if([](std::shared_ptr<ir::LocalVariable> ir_local_variable) {
            return ir_local_variable->annotation_dict.count(DISABLE_REFERENCE_COUNT);
        });

        for (auto ir_local_variable : ir_local_variables) {
            auto ir_builder = makeIRbuilder();
            ir_builder->pushBlock(ir_local_variable->parent_block);
            auto iter =
                std::find(RANGE(ir_local_variable->parent_block->values), ir_local_variable);
            PRAJNA_ASSERT(iter != ir_local_variable->parent_block->values.end());
            // 应该在变量后面插入
            ir_builder->inserter_iterator = std::next(iter);

            initializeVariableLikedCallback(ir_local_variable, ir_builder);
        }
    }

    return ir_module;
}

inline void insertDestroyLocalVariableForBlock(std::shared_ptr<ir::Block> ir_block) {
    auto ir_builder = lowering::IrBuilder::create();
    ir_builder->pushBlock(ir_block);
    ir_builder->inserter_iterator =
        std::find_if(RANGE(ir_block->values), [](auto x) { return ir::IsTerminated(x); });

    std::list<std::shared_ptr<ir::LocalVariable>> ir_local_variable_list;

    for (auto iter = ir_block->values.begin(); iter != ir_builder->inserter_iterator; ++iter) {
        auto ir_value = *iter;
        if (auto ir_variable = cast<ir::LocalVariable>(ir_value)) {
            ir_local_variable_list.push_back(ir_variable);
        }

        if (auto ir_block = cast<ir::Block>(ir_value)) {
            insertDestroyLocalVariableForBlock(ir_block);
        }

        if (auto ir_if = cast<ir::If>(ir_value)) {
            insertDestroyLocalVariableForBlock(ir_if->trueBlock());
            insertDestroyLocalVariableForBlock(ir_if->falseBlock());
        }

        if (auto ir_while = cast<ir::While>(ir_value)) {
            insertDestroyLocalVariableForBlock(ir_while->loopBlock());
        }

        if (auto ir_for = cast<ir::For>(ir_value)) {
            insertDestroyLocalVariableForBlock(ir_for->loopBlock());
        }
    }

    ir_local_variable_list.remove_if([](std::shared_ptr<ir::LocalVariable> ir_local_variable) {
        return ir_local_variable->annotation_dict.count(DISABLE_REFERENCE_COUNT);
    });
    while (not ir_local_variable_list.empty()) {
        auto ir_local_variable = ir_local_variable_list.front();
        destroyVariableLikedCallback(ir_local_variable, ir_builder);
        ir_local_variable_list.pop_front();
    }
}

inline std::shared_ptr<ir::Module> insertLoacalVariableScopeDecrementReferenceCount(
    std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        for (auto ir_block : ir_function->blocks) {
            insertDestroyLocalVariableForBlock(ir_block);
        }
    }

    return ir_module;
}
/// @brief
/// @param ir_module
/// @note 需要再destroyVariable前面执行
/// @return
inline std::shared_ptr<ir::Module> insertVariableIncrementReferenceCount(
    std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_write_variable_likes =
            utility::getValuesInFunction<ir::WriteVariableLiked>(ir_function);

        ir_write_variable_likes.remove_if(
            [](std::shared_ptr<ir::WriteVariableLiked> ir_write_variable_liked) {
                return ir_write_variable_liked->annotation_dict.count(DISABLE_REFERENCE_COUNT);
            });
        for (auto ir_write_variable_liked : ir_write_variable_likes) {
            auto ir_builder = makeIRbuilder();
            ir_builder->pushBlock(ir_write_variable_liked->parent_block);
            auto iter = std::find(RANGE(ir_write_variable_liked->parent_block->values),
                                  ir_write_variable_liked);
            ir_builder->inserter_iterator = iter;
            destroyVariableLikedCallback(ir_write_variable_liked->variable(), ir_builder);

            copyVariableLikedCallback(ir_write_variable_liked->value(), ir_builder);
        }
    }

    return ir_module;
}

inline std::shared_ptr<ir::Module> insertDestroyForCall(std::shared_ptr<ir::Module> ir_module) {
    auto ir_calls = utility::getValuesInModule<ir::Call>(ir_module);
    for (auto ir_call : ir_calls) {
        if (lowering::hasReferenceCountable(ir_call->type)) {
            auto ir_builder = makeIRbuilder();
            ir_builder->pushBlock(ir_call->parent_block);

            // 插入copy函数时, 需要normlizeVariableLiked, 这样会产生一个WriteVaribleLiked引用它
            PRAJNA_ASSERT(ir_call->instruction_with_index_list.size() <= 1);
            if (ir_call->instruction_with_index_list.empty()) {
                auto iter = std::find(RANGE(ir_call->parent_block->values), ir_call);
                ir_builder->inserter_iterator = std::next(iter);
                destroyVariableLikedCallback(ir_call, ir_builder);
            } else {
                auto ir_instruction_use_call =
                    ir_call->instruction_with_index_list.back().instruction;
                // 返回值则不进行destroy处理, 相应的在return时也不会有copy处理,
                // 因为return后再destroy是无效的.
                if (is<ir::Return>(ir_instruction_use_call)) {
                    continue;
                }
                // 如果是临时变量, 那应该在使用完临时变量后插入
                if (ir_instruction_use_call->annotation_dict.count("DisableReferenceCount")) {
                    auto ir_write_variable_liked =
                        cast<ir::WriteVariableLiked>(ir_instruction_use_call);
                    auto ir_object_pointer = ir_write_variable_liked->variable()
                                                 ->instruction_with_index_list.back()
                                                 .instruction;
                    ir_instruction_use_call =
                        ir_object_pointer->instruction_with_index_list.back().instruction;
                    PRAJNA_ASSERT(is<ir::Call>(ir_instruction_use_call));
                }

                ir_builder->inserter_iterator =
                    std::next(ir_instruction_use_call->GetBlockIterator());
                destroyVariableLikedCallback(ir_call, ir_builder);
            }
        }
    }

    return ir_module;
}

inline std::shared_ptr<ir::Module> insertCopyForReturn(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_returns = utility::getValuesInFunction<ir::Return>(ir_function);
        for (auto ir_return : ir_returns) {
            if (lowering::hasReferenceCountable(ir_return->type)) {
                // 如果是Call则不做操作, 和insertDestroyForCall里的逻辑对应
                if (is<ir::Call>(ir_return->value())) {
                    continue;
                }

                auto ir_builder = makeIRbuilder();
                ir_builder->pushBlock(ir_return->parent_block);
                auto iter = std::find(RANGE(ir_return->parent_block->values), ir_return);
                ir_builder->inserter_iterator = iter;
                copyVariableLikedCallback(ir_return->value(), ir_builder);
            }
        }
    }

    return ir_module;
}

}  // namespace

inline std::shared_ptr<ir::Module> insertReferenceCount(std::shared_ptr<ir::Module> ir_module) {
    ir_module = insertDestroyForCall(ir_module);  // 放在第一个, 否则插入的指令会影响
    ir_module = insertLocalVariableRegisterReferenceCount(ir_module);
    ir_module = insertVariableIncrementReferenceCount(ir_module);
    ir_module = insertCopyForReturn(ir_module);
    ir_module = insertLoacalVariableScopeDecrementReferenceCount(ir_module);
    return ir_module;
}

}  // namespace prajna::transform
