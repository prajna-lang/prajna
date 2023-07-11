#pragma once

#include <memory>

#include "prajna/ir/ir.hpp"
#include "prajna/logger.hpp"
#include "prajna/lowering/statement_lowering_visitor.hpp"
#include "prajna/transform/utility.hpp"
#include "prajna/transform/verify.hpp"

namespace prajna::transform {

inline bool InlineCheck(std::shared_ptr<ir::Function> ir_function) {
    return ir_function->annotation_dict.count("inline");
}

/// @brief 内联其他模块的函数, 这里只处理其他模块的, 本模块的函数内联由llvm完成,
/// 目前仅考虑简单的情况, 后续还需要考虑把函数作为参数传递的高阶函数的优化,
/// 把函数作为变量传递后调用的情况后续有需求再做处理, 因为只有编译时确定函数调用的才能内联
inline bool InlineFunction(std::shared_ptr<ir::Module> ir_module) {
    bool re = false;
    for (auto ir_function : ir_module->functions) {
        VerifyFunction(ir_function);

        auto ir_calls = utility::GetValuesInFunction<ir::Call>(ir_function);
        for (auto ir_call : ir_calls) {
            auto ir_callee = cast<ir::Function>(ir_call->Function());
            if (!ir_callee || ir_callee->IsDeclaration()) continue;
            if (!InlineCheck(ir_callee)) continue;

            re = true;
            auto iter = std::find(RANGE(ir_call->parent_block->values), ir_call);
            auto function_cloner = ir::FunctionCloner::Create(ir_module);
            function_cloner->shallow = true;
            auto ir_new_callee = cast<ir::Function>(ir_callee->Clone(function_cloner));
            VerifyFunction(ir_callee);
            VerifyFunction(ir_new_callee);

            // 不能加入到module里
            function_cloner->module->functions.remove(ir_new_callee);

            auto ir_builder = lowering::IrBuilder::Create();
            ir_builder->PushBlock(ir_call->parent_block);
            ir_builder->inserter_iterator = iter;

            auto ir_parameter_iter = ir_new_callee->parameters.begin();
            for (int i = 0; i < ir_call->ArgumentSize(); ++i, ++ir_parameter_iter) {
                // 置换形参为实参
                auto ir_parameter = *ir_parameter_iter;
                auto ir_argument = ir_call->Argument(i);
                auto ir_parameter_inst_idx = ir_parameter->instruction_with_index_list;
                for (auto &inst_idx : ir_parameter_inst_idx) {
                    auto [inst, op_idx] = inst_idx;
                    inst->operand(op_idx, ir_argument);
                }
                ir_parameter->Finalize();
            }

            auto ir_returns = utility::GetValuesInFunction<ir::Return>(ir_new_callee);
            bool has_return =
                !ir_returns.empty() && !Is<ir::VoidValue>(ir_returns.front()->Value());
            std::shared_ptr<ir::LocalVariable> ir_return_variable = nullptr;
            for (auto ir_return : ir_returns) {
                if (Is<ir::VoidValue>(ir_return->Value())) {
                    auto ir_void = ir_return->Value();
                    utility::RemoveFromParent(ir_return);
                    ir_return->Finalize();
                    utility::RemoveFromParent(ir_void);
                    ir_void->Finalize();
                } else {
                    // 写入返回值到变量
                    ir_return_variable = ir_builder->Create<ir::LocalVariable>(
                        ir_new_callee->function_type->return_type);
                    auto ir_return_builder = lowering::IrBuilder::Create();
                    auto ir_return_iter = ir_return->GetBlockIterator();
                    ir_return_builder->PushBlock(ir_return->parent_block);
                    ir_return_builder->inserter_iterator = ir_return_iter;
                    ir_return_builder->Create<ir::WriteVariableLiked>(ir_return->Value(),
                                                                      ir_return_variable);
                    utility::RemoveFromParent(ir_return);
                    ir_return->Finalize();
                }
            }

            ir_new_callee->blocks.front()->parent_function = nullptr;
            ir_call->parent_block->insert(iter, ir_new_callee->blocks.front());

            auto ir_call_inst_idx = ir_call->instruction_with_index_list;
            for (auto [inst, op_idx] : ir_call_inst_idx) {
                PRAJNA_ASSERT(ir_return_variable);
                inst->operand(op_idx, ir_return_variable);
            }

            utility::RemoveFromParent(ir_call);
            ir_call->Finalize();
        }

        VerifyFunction(ir_function);
    }

    return re;
}
}  // namespace prajna::transform
