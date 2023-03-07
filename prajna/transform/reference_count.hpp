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

bool isReferenceCount(std::shared_ptr<ir::Type> ir_type) {
    auto ir_interface_implement = ir_type->interfaces["ReferenceCount"];
    return ir_interface_implement != nullptr;
}

bool isReferenceCountAble(std::shared_ptr<ir::Type> ir_type) {
    if (isReferenceCount(ir_type)) {
        return true;
    }

    if (auto ir_struct_type = cast<ir::StructType>(ir_type)) {
        for (auto ir_field : ir_struct_type->fields) {
            if (isReferenceCountAble(ir_field->type)) {
                return true;
            }
        }
    }

    return false;
}

const std::string INSERTED_FLAG = "INSERTED_FLAG";

std::shared_ptr<lowering::IrBuilder> makeIRbuilder() {
    auto ir_builder = lowering::IrBuilder::create();
    ir_builder->create_callback = [](std::shared_ptr<ir::Value> ir_value) {
        ir_value->annotations[INSERTED_FLAG].push_back("none");
    };
    return ir_builder;
}

inline void initializeVariableLikedCallback(std::shared_ptr<ir::VariableLiked> ir_variable_liked,
                                            std::shared_ptr<lowering::IrBuilder> ir_builder) {
    auto ir_type = ir_variable_liked->type;
    if (isReferenceCountAble(ir_type)) {
        if (auto is_struct_type = cast<ir::StructType>(ir_type)) {
            for (auto ir_field : is_struct_type->fields) {
                if (isReferenceCountAble(ir_field->type)) {
                    auto ir_access_field =
                        ir_builder->create<ir::AccessField>(ir_variable_liked, ir_field);
                    initializeVariableLikedCallback(ir_access_field, ir_builder);
                }
            }
        }

        if (isReferenceCount(ir_type)) {
            auto ir_function = ir::getFunctionByName(
                ir_type->interfaces["ReferenceCount"]->functions, "initialize");
            ir_builder->callMemberFunction(ir_variable_liked, ir_function, {});
        };
    }
}

inline void destroyVariableLikedCallback(std::shared_ptr<ir::Value> ir_value,
                                         std::shared_ptr<lowering::IrBuilder> ir_builder) {
    auto ir_type = ir_value->type;
    if (isReferenceCountAble(ir_type)) {
        auto ir_variable_liked = ir_builder->variableLikedNormalize(ir_value);
        if (auto is_struct_type = cast<ir::StructType>(ir_type)) {
            for (auto ir_field : is_struct_type->fields) {
                if (isReferenceCountAble(ir_field->type)) {
                    auto ir_access_field =
                        ir_builder->create<ir::AccessField>(ir_variable_liked, ir_field);
                    destroyVariableLikedCallback(ir_access_field, ir_builder);
                }
            }
        }

        if (isReferenceCount(ir_type)) {
            auto ir_function = ir::getFunctionByName(
                ir_type->interfaces["ReferenceCount"]->functions, "decrementReferenceCount");
            ir_builder->callMemberFunction(ir_variable_liked, ir_function, {});
        };
    }
}

/// @brief
/// @param ir_value
/// @param ir_builder
/// @return
inline void copyVariableLikedCallback(std::shared_ptr<ir::Value> ir_value,
                                      std::shared_ptr<lowering::IrBuilder> ir_builder) {
    auto ir_type = ir_value->type;
    if (isReferenceCountAble(ir_type)) {
        auto ir_variable_liked = ir_builder->variableLikedNormalize(ir_value);
        if (auto ir_struct_type = cast<ir::StructType>(ir_type)) {
            for (auto ir_field : ir_struct_type->fields) {
                if (isReferenceCountAble(ir_field->type)) {
                    auto ir_access_field =
                        ir_builder->create<ir::AccessField>(ir_variable_liked, ir_field);
                    copyVariableLikedCallback(ir_access_field, ir_builder);
                }
            }
        }

        if (isReferenceCount(ir_type)) {
            auto ir_function = ir::getFunctionByName(
                ir_type->interfaces["ReferenceCount"]->functions, "incrementReferenceCount");
            ir_builder->callMemberFunction(ir_variable_liked, ir_function, {});
        }
    }
}

inline std::shared_ptr<ir::Module> insertLocalVariableRegisterReferenceCount(
    std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_local_variables = utility::getValuesInFunction<ir::LocalVariable>(ir_function);

        for (auto ir_local_variable : ir_local_variables) {
            auto ir_builder = makeIRbuilder();
            ir_builder->pushBlock(ir_local_variable->parent_block);
            auto iter =
                std::find(RANGE(ir_local_variable->parent_block->values), ir_local_variable);
            ir_builder->inserter_iterator = iter;

            initializeVariableLikedCallback(ir_local_variable, ir_builder);
        }
    }

    return ir_module;
}

inline void insertDestroyLocalVariableForBlock(std::shared_ptr<ir::Block> ir_block) {
    auto iter_return =
        std::find_if(RANGE(ir_block->values), [](auto x) { return is<ir::Return>(x); });
    auto ir_builder = lowering::IrBuilder::create();
    ir_builder->pushBlock(ir_block);
    ir_builder->inserter_iterator = iter_return;

    std::list<std::shared_ptr<ir::LocalVariable>> ir_local_variable_list;

    for (auto iter = ir_block->values.begin(); iter != iter_return; ++iter) {
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
        return ir_local_variable->annotations.count(INSERTED_FLAG);
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
            [](std::shared_ptr<ir::WriteVariableLiked> ir_variable_liked) {
                return ir_variable_liked->annotations.count(INSERTED_FLAG);
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

inline std::shared_ptr<ir::Module> insertCopyAndDecrementReferenceCountForCallandReturn(
    std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        auto ir_calls = utility::getValuesInFunction<ir::Call>(ir_function);
        for (auto ir_call : ir_calls) {
            if (not isReferenceCountAble(ir_call->type)) continue;

            auto ir_builder = makeIRbuilder();
            ir_builder->pushBlock(ir_call->parent_block);

            // 插入copy函数时, 需要normlizeVariableLiked, 这样会产生一个WriteVaribleLiked引用它
            // PRAJNA_ASSERT(ir_call->instruction_with_index_list.size() <= 1);
            if (ir_call->instruction_with_index_list.empty()) {
                auto iter = std::find(RANGE(ir_call->parent_block->values), ir_call);
                ir_builder->inserter_iterator = std::next(iter);
            } else {
                auto ir_instruction = ir_call->instruction_with_index_list.front().instruction;

                // 返回值则不进行destroy处理, 相应的在return时也不会有copy处理,
                // 因为return后再destroy是无效的.
                if (is<ir::Return>(ir_instruction)) continue;

                // 使用完才能destroy
                auto iter = std::find(RANGE(ir_instruction->parent_block->values), ir_instruction);
                ir_builder->inserter_iterator = std::next(iter);
            }

            destroyVariableLikedCallback(ir_call, ir_builder);
        }

        auto ir_returns = utility::getValuesInFunction<ir::Return>(ir_function);
        for (auto ir_return : ir_returns) {
            if (not isReferenceCountAble(ir_return->type)) continue;

            auto ir_builder = makeIRbuilder();
            ir_builder->pushBlock(ir_return->parent_block);
            auto iter = std::find(RANGE(ir_return->parent_block->values), ir_return);
            ir_builder->inserter_iterator = iter;

            // 和call的处理相对应
            if (is<ir::Call>(ir_return->value())) continue;

            copyVariableLikedCallback(ir_return->value(), ir_builder);
        }
    }

    return ir_module;
}

}  // namespace

inline std::shared_ptr<ir::Module> insertReferenceCount(std::shared_ptr<ir::Module> ir_module) {
    ir_module = insertLocalVariableRegisterReferenceCount(ir_module);
    ir_module = insertVariableIncrementReferenceCount(ir_module);
    ir_module = insertCopyAndDecrementReferenceCountForCallandReturn(ir_module);
    ir_module = insertLoacalVariableScopeDecrementReferenceCount(ir_module);
    return ir_module;
}

}  // namespace prajna::transform
