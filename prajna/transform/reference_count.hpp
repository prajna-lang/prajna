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

std::shared_ptr<lowering::IrBuilder> MakeIRbuilder() {
    auto ir_builder = lowering::IrBuilder::Create();
    ir_builder->create_callback = [](std::shared_ptr<ir::Value> ir_value) {
        ir_value->annotation_dict[DISABLE_REFERENCE_COUNT];
    };
    return ir_builder;
}

inline void InsertLocalVariableRegisterReferenceCount(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_local_variables = utility::GetValuesInFunction<ir::LocalVariable>(ir_function);
        ir_local_variables.remove_if([](std::shared_ptr<ir::LocalVariable> ir_local_variable) {
            return ir_local_variable->annotation_dict.count(DISABLE_REFERENCE_COUNT);
        });

        for (auto ir_local_variable : ir_local_variables) {
            auto ir_builder = MakeIRbuilder();
            ir_builder->PushBlock(ir_local_variable->parent_block);
            auto iter =
                std::find(RANGE(ir_local_variable->parent_block->values), ir_local_variable);
            PRAJNA_ASSERT(iter != ir_local_variable->parent_block->values.end());
            // 应该在变量后面插入
            ir_builder->inserter_iterator = std::next(iter);

            InitializeVariableLikedCallback(ir_local_variable, ir_builder);
        }
    }
}

inline void InsertDestroyLocalVariableForBlock(std::shared_ptr<ir::Block> ir_block) {
    auto ir_builder = lowering::IrBuilder::Create();
    ir_builder->PushBlock(ir_block);
    ir_builder->inserter_iterator =
        std::find_if(RANGE(ir_block->values), [](auto x) { return ir::IsTerminated(x); });

    std::list<std::shared_ptr<ir::LocalVariable>> ir_local_variable_list;

    for (auto iter = ir_block->values.begin(); iter != ir_builder->inserter_iterator; ++iter) {
        auto ir_value = *iter;
        if (auto ir_variable = cast<ir::LocalVariable>(ir_value)) {
            ir_local_variable_list.push_back(ir_variable);
        }

        if (auto ir_block = cast<ir::Block>(ir_value)) {
            InsertDestroyLocalVariableForBlock(ir_block);
        }

        if (auto ir_if = cast<ir::If>(ir_value)) {
            InsertDestroyLocalVariableForBlock(ir_if->TrueBlock());
            InsertDestroyLocalVariableForBlock(ir_if->FalseBlock());
        }

        if (auto ir_while = cast<ir::While>(ir_value)) {
            InsertDestroyLocalVariableForBlock(ir_while->LoopBlock());
        }

        if (auto ir_for = cast<ir::For>(ir_value)) {
            InsertDestroyLocalVariableForBlock(ir_for->LoopBlock());
        }
    }

    ir_local_variable_list.remove_if([](std::shared_ptr<ir::LocalVariable> ir_local_variable) {
        return ir_local_variable->annotation_dict.count(DISABLE_REFERENCE_COUNT);
    });
    while (not ir_local_variable_list.empty()) {
        auto ir_local_variable = ir_local_variable_list.front();
        DestroyVariableLikedCallback(ir_local_variable, ir_builder);
        ir_local_variable_list.pop_front();
    }
}

inline void InsertLoacalVariableScopeDecrementReferenceCount(
    std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        for (auto ir_block : ir_function->blocks) {
            InsertDestroyLocalVariableForBlock(ir_block);
        }
    }
}
/// @brief
/// @param ir_module
/// @note 需要再destroyVariable前面执行
/// @return
inline void InsertVariableIncrementReferenceCount(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_write_variable_likes =
            utility::GetValuesInFunction<ir::WriteVariableLiked>(ir_function);

        ir_write_variable_likes.remove_if(
            [](std::shared_ptr<ir::WriteVariableLiked> ir_write_variable_liked) {
                return ir_write_variable_liked->annotation_dict.count(DISABLE_REFERENCE_COUNT);
            });
        for (auto ir_write_variable_liked : ir_write_variable_likes) {
            auto ir_builder = MakeIRbuilder();
            ir_builder->PushBlock(ir_write_variable_liked->parent_block);
            auto iter = std::find(RANGE(ir_write_variable_liked->parent_block->values),
                                  ir_write_variable_liked);
            ir_builder->inserter_iterator = iter;
            DestroyVariableLikedCallback(ir_write_variable_liked->variable(), ir_builder);

            CopyVariableLikedCallback(ir_write_variable_liked->value(), ir_builder);
        }
    }
}

inline void InsertDestroyForCall(std::shared_ptr<ir::Module> ir_module) {
    auto ir_calls = utility::GetValuesInModule<ir::Call>(ir_module);
    for (auto ir_call : ir_calls) {
        if (lowering::HasReferenceCountable(ir_call->type)) {
            auto ir_builder = MakeIRbuilder();
            ir_builder->PushBlock(ir_call->parent_block);

            // 插入copy函数时, 需要normlizeVariableLiked, 这样会产生一个WriteVaribleLiked引用它
            PRAJNA_ASSERT(ir_call->instruction_with_index_list.size() <= 1);
            if (ir_call->instruction_with_index_list.empty()) {
                auto iter = std::find(RANGE(ir_call->parent_block->values), ir_call);
                ir_builder->inserter_iterator = std::next(iter);
                DestroyVariableLikedCallback(ir_call, ir_builder);
            } else {
                auto ir_instruction_use_call =
                    ir_call->instruction_with_index_list.back().instruction;
                // 返回值则不进行destroy处理, 相应的在return时也不会有copy处理,
                // 因为return后再destroy是无效的.
                if (Is<ir::Return>(ir_instruction_use_call)) {
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
                    PRAJNA_ASSERT(Is<ir::Call>(ir_instruction_use_call));
                }

                ir_builder->inserter_iterator =
                    std::next(ir_instruction_use_call->GetBlockIterator());
                DestroyVariableLikedCallback(ir_call, ir_builder);
            }
        }
    }
}

inline void InsertCopyForReturn(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_returns = utility::GetValuesInFunction<ir::Return>(ir_function);
        for (auto ir_return : ir_returns) {
            if (lowering::HasReferenceCountable(ir_return->type)) {
                // 如果是Call则不做操作, 和insertDestroyForCall里的逻辑对应
                if (Is<ir::Call>(ir_return->value())) {
                    continue;
                }

                auto ir_builder = MakeIRbuilder();
                ir_builder->PushBlock(ir_return->parent_block);
                auto iter = std::find(RANGE(ir_return->parent_block->values), ir_return);
                ir_builder->inserter_iterator = iter;
                CopyVariableLikedCallback(ir_return->value(), ir_builder);
            }
        }
    }
}

}  // namespace

inline void InsertReferenceCount(std::shared_ptr<ir::Module> ir_module) {
    InsertDestroyForCall(ir_module);  // 放在第一个, 否则插入的指令会影响
    InsertLocalVariableRegisterReferenceCount(ir_module);
    InsertVariableIncrementReferenceCount(ir_module);
    InsertCopyForReturn(ir_module);
    InsertLoacalVariableScopeDecrementReferenceCount(ir_module);
}

}  // namespace prajna::transform
