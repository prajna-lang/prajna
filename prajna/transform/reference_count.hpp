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

inline void InsertLocalVariableInitialize(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_local_variables = utility::GetAll<ir::LocalVariable>(ir_function);
        ir_local_variables.remove_if([](std::shared_ptr<ir::LocalVariable> ir_local_variable) {
            return ir_local_variable->annotation_dict.count(DISABLE_REFERENCE_COUNT);
        });

        for (auto ir_local_variable : ir_local_variables) {
            auto ir_builder = MakeIRbuilder();
            auto parent = ir_local_variable->GetParentBlock();
            ir_builder->PushBlock(parent);
            auto iter = std::find(RANGE((*parent)), ir_local_variable);
            PRAJNA_ASSERT(iter != parent->end());
            // 应该在变量后面插入
            ir_builder->inserter_iterator = std::next(iter);

            InitializeVariableLikedCallback(ir_local_variable, ir_builder);
        }
    }
}

inline void InsertFinalizeLocalVariableForBlock(std::shared_ptr<ir::Block> ir_block) {
    auto ir_builder = lowering::IrBuilder::Create();
    ir_builder->PushBlock(ir_block);
    ir_builder->inserter_iterator =
        std::find_if(RANGE((*ir_block)), [](auto x) { return ir::IsTerminated(x); });

    std::list<std::shared_ptr<ir::LocalVariable>> ir_local_variable_list;

    for (auto iter = ir_block->begin(); iter != ir_builder->inserter_iterator; ++iter) {
        auto ir_value = *iter;
        if (auto ir_variable = Cast<ir::LocalVariable>(ir_value)) {
            ir_local_variable_list.push_back(ir_variable);
        }

        if (auto ir_block = Cast<ir::Block>(ir_value)) {
            InsertFinalizeLocalVariableForBlock(ir_block);
        }

        if (auto ir_if = Cast<ir::If>(ir_value)) {
            InsertFinalizeLocalVariableForBlock(ir_if->TrueBlock());
            InsertFinalizeLocalVariableForBlock(ir_if->FalseBlock());
        }

        if (auto ir_while = Cast<ir::While>(ir_value)) {
            InsertFinalizeLocalVariableForBlock(ir_while->LoopBlock());
        }

        if (auto ir_for = Cast<ir::For>(ir_value)) {
            InsertFinalizeLocalVariableForBlock(ir_for->LoopBlock());
        }
    }

    ir_local_variable_list.remove_if([](std::shared_ptr<ir::LocalVariable> ir_local_variable) {
        return ir_local_variable->annotation_dict.count(DISABLE_REFERENCE_COUNT);
    });
    while (!ir_local_variable_list.empty()) {
        auto ir_local_variable = ir_local_variable_list.front();
        FinalizeVariableLikedCallback(ir_local_variable, ir_builder);
        ir_local_variable_list.pop_front();
    }
}

inline void InsertLoacalVariableScopeDecrementReferenceCount(
    std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        for (auto ir_block : ir_function->blocks) {
            InsertFinalizeLocalVariableForBlock(ir_block);
        }
    }
}
/// @brief
/// @param ir_module
/// @note 需要再destroyVariable前面执行
/// @return
inline void InsertVariableIncrementReferenceCount(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_write_variable_likes = utility::GetAll<ir::WriteVariableLiked>(ir_function);

        ir_write_variable_likes.remove_if(
            [](std::shared_ptr<ir::WriteVariableLiked> ir_write_variable_liked) {
                return ir_write_variable_liked->annotation_dict.count(DISABLE_REFERENCE_COUNT);
            });
        for (auto ir_write_variable_liked : ir_write_variable_likes) {
            auto ir_builder = MakeIRbuilder();
            auto parent = ir_write_variable_liked->GetParentBlock();
            ir_builder->PushBlock(parent);
            auto iter = std::find(RANGE((*parent)), ir_write_variable_liked);
            ir_builder->inserter_iterator = iter;
            FinalizeVariableLikedCallback(ir_write_variable_liked->variable(), ir_builder);

            CopyVariableLikedCallback(ir_write_variable_liked->Value(), ir_builder);
        }
    }
}

inline void InsertDestroyForCall(std::shared_ptr<ir::Module> ir_module) {
    auto ir_calls = utility::GetAll<ir::Call>(ir_module);
    for (auto ir_call : ir_calls) {
        if (lowering::HasFinalize(ir_call->type)) {
            auto ir_builder = MakeIRbuilder();
            ir_builder->PushBlock(ir_call->GetParentBlock());

            // 插入copy函数时, 需要normlizeVariableLiked, 这样会产生一个WriteVaribleLiked引用它
            PRAJNA_ASSERT(ir_call->instruction_with_index_list.size() <= 1);
            if (ir_call->instruction_with_index_list.empty()) {
                auto iter = std::find(RANGE((*ir_call->GetParentBlock())), ir_call);
                ir_builder->inserter_iterator = std::next(iter);
                FinalizeVariableLikedCallback(ir_call, ir_builder);
            } else {
                auto ir_instruction_use_call =
                    Lock(ir_call->instruction_with_index_list.back().instruction);
                // 返回值则不进行destroy处理, 相应的在return时也不会有copy处理,
                // 因为return后再destroy是无效的.
                if (Is<ir::Return>(ir_instruction_use_call)) {
                    continue;
                }
                // 如果是临时变量, 那应该在使用完临时变量后插入
                if (ir_instruction_use_call->annotation_dict.count("DisableReferenceCount")) {
                    auto ir_write_variable_liked =
                        Cast<ir::WriteVariableLiked>(ir_instruction_use_call);
                    auto ir_object_pointer = Lock(ir_write_variable_liked->variable()
                                                      ->instruction_with_index_list.back()
                                                      .instruction);
                    ir_instruction_use_call =
                        Lock(ir_object_pointer->instruction_with_index_list.back().instruction);
                    PRAJNA_ASSERT(Is<ir::Call>(ir_instruction_use_call));
                }

                ir_builder->inserter_iterator =
                    std::next(ir_instruction_use_call->GetBlockIterator());
                FinalizeVariableLikedCallback(ir_call, ir_builder);
            }
        }
    }
}

inline void InsertCopyForReturn(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_returns = utility::GetAll<ir::Return>(ir_function);
        for (auto ir_return : ir_returns) {
            if (lowering::HasCopy(ir_return->type)) {
                // 如果是Call则不做操作, 和insertDestroyForCall里的逻辑对应
                if (Is<ir::Call>(ir_return->Value())) {
                    continue;
                }

                auto ir_builder = MakeIRbuilder();
                auto parent = ir_return->GetParentBlock();
                ir_builder->PushBlock(parent);
                auto iter = std::find(RANGE((*parent)), ir_return);
                ir_builder->inserter_iterator = iter;
                CopyVariableLikedCallback(ir_return->Value(), ir_builder);
            }
        }
    }
}

}  // namespace

inline void InsertReferenceCount(std::shared_ptr<ir::Module> ir_module) {
    InsertDestroyForCall(ir_module);  // 放在第一个, 否则插入的指令会影响
    InsertLocalVariableInitialize(ir_module);
    InsertVariableIncrementReferenceCount(ir_module);
    InsertCopyForReturn(ir_module);
    InsertLoacalVariableScopeDecrementReferenceCount(ir_module);
}

}  // namespace prajna::transform
