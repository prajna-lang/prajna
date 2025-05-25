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

inline auto ConvertGpuForToKernelCall(std::shared_ptr<ir::For> ir_gpu_for, int64_t idx) {
    auto ir_module = ir_gpu_for->GetParentFunction()->parent_module;
    auto ir_nvptx_module = ir_module->modules[ir::Target::nvptx];

    auto ir_captured_variables_list =
        utility::CaptureExternalVariablesInBlock(ir_gpu_for->LoopBlock());
    // 将index, first, last
    ir_captured_variables_list.remove(ir_gpu_for->IndexVariable());
    utility::RemoveFromParent(ir_gpu_for->IndexVariable());

    std::list<std::shared_ptr<ir::Type>> ir_argument_types;
    // 加入first和last
    ir_argument_types.push_back(ir_gpu_for->First()->type);
    ir_argument_types.push_back(ir_gpu_for->Last()->type);
    std::transform(RANGE(ir_captured_variables_list), std::back_inserter(ir_argument_types),
                   [=](std::shared_ptr<ir::Value> ir_value) {
                       if (utility::IsHostTensorType(ir_value->type, ir_module)) {
                           return utility::GetGpuTensorTypeOfHostTensorType(ir_value->type,
                                                                            ir_module);
                       } else {
                           return ir_value->type;
                       }
                   });

    auto ir_kernel_function_type =
        ir::FunctionType::Create(ir_argument_types, ir::VoidType::Create());
    auto ir_kernel_function = ir::Function::Create(ir_kernel_function_type);
    ir_kernel_function->fullname = ConcatFullname(ir_gpu_for->GetParentFunction()->fullname,
                                                  "auto_created_gpu_for_" + std::to_string(idx));
    ir_kernel_function->name = ir_kernel_function->fullname;
    ir_kernel_function_type->fullname = ir_kernel_function->fullname;
    ir_kernel_function_type->name = ir_kernel_function->fullname;
    ir_kernel_function->parent_module = ir_nvptx_module;
    ir_kernel_function->annotation_dict["target"].push_back("nvptx");
    ir_kernel_function->annotation_dict.insert({"kernel", {}});
    ir_nvptx_module->functions.push_back(ir_kernel_function);

    auto ir_block = ir::Block::Create();
    ir_block->parent = ir_kernel_function;
    ir_kernel_function->blocks.push_back(ir_block);

    auto ir_builder = lowering::IrBuilder::Create();
    ir_builder->PushBlock(ir_block);
    ir_builder->inserter_iterator = ir_block->end();

    std::unordered_map<std::shared_ptr<ir::Value>, std::shared_ptr<ir::Value>> variables_dict;
    auto iter_captured_variable = ir_captured_variables_list.begin();
    auto iter_parameter = ir_kernel_function->parameters.begin();
    auto ir_first = *iter_parameter;
    variables_dict[ir_gpu_for->First()] = ir_first;
    ++iter_parameter;
    auto ir_last = *iter_parameter;
    variables_dict[ir_gpu_for->Last()] = ir_last;
    ++iter_parameter;
    for (; iter_parameter != ir_kernel_function->parameters.end();
         ++iter_parameter, ++iter_captured_variable) {
        // 需要搞成临时变量, 这样才会原来的变量对应, 以便访问成员函数等
        variables_dict[*iter_captured_variable] =
            ir_builder->VariableLikedNormalize(*iter_parameter);
    }

    // 将捕获的变量置换为参数对应的临时变量
    for (auto ir_captured_variable : ir_captured_variables_list) {
        auto instruction_with_index_inner_list = ir_captured_variable->instruction_with_index_list;
        instruction_with_index_inner_list.remove_if(
            [=](ir::InstructionAndOperandIndex instruction_with_operand_index) -> bool {
                return Lock(instruction_with_operand_index.instruction)->GetRootBlock() !=
                       ir_gpu_for->LoopBlock();
            });
        for (auto instruction_with_operand_index : instruction_with_index_inner_list) {
            auto ir_instruction = Lock(instruction_with_operand_index.instruction);
            auto operand_index = instruction_with_operand_index.operand_index;
            PRAJNA_ASSERT(variables_dict.count(ir_captured_variable));
            ir_instruction->SetOperand(operand_index, variables_dict[ir_captured_variable]);
        }
    }

    auto ir_index = ir_builder->Create<ir::LocalVariable>(ir_builder->GetInt64Type());
    ir_index->name = "i";
    for (auto inst_with_idx : Clone(ir_gpu_for->IndexVariable()->instruction_with_index_list)) {
        auto ir_instruction = Lock(inst_with_idx.instruction);
        // ir_gpu_for将被移除, 不能再使用ir_index
        if (ir_instruction == ir_gpu_for) continue;

        auto operand_index = inst_with_idx.operand_index;
        ir_instruction->SetOperand(operand_index, ir_index);
    }

    {
        std::string code =
            "var thread_idx = gpu::ThreadIndex();"
            "var block_idx = gpu::BlockIndex();"
            "var block_shape = gpu::BlockShape();"
            "var grid_shape = gpu::GridShape();"
            "i = first + thread_idx[0] + block_idx[0] * block_shape[0];"
            "while (i < last){"
            "   i = i + block_shape[0] * grid_shape[0];"
            "}";

        auto logger = Logger::Create(code);
        auto symbol_table = ir_module->symbol_table;
        auto statement_lowering_visitor =
            lowering::StatementLoweringVisitor::Create(symbol_table, logger, nullptr, nullptr);
        auto ir_tmp_builder = statement_lowering_visitor->ir_builder;
        ir_tmp_builder->function_stack.push(ir_kernel_function);
        ir_tmp_builder->PushSymbolTable();
        ir_tmp_builder->PushBlock(ir_block);

        ir_tmp_builder->symbol_table->Set(ir_index, "i");
        ir_tmp_builder->symbol_table->Set(ir_first, "first");
        ir_tmp_builder->symbol_table->Set(ir_last, "last");

        auto ast = prajna::parser::parse(code, "//None", logger);

        statement_lowering_visitor->Apply(ast);

        // 插入ir_gpu_for里的逻辑
        auto ir_kernel_while = Cast<ir::While>(*std::prev(ir_block->end(), 2));
        PRAJNA_ASSERT(ir_kernel_while);
        ir_kernel_while->ConditionBlock()->parent.reset();
        auto ir_kernel_while_loop_block = Cast<ir::Block>(ir_kernel_while->LoopBlock()->front());
        PRAJNA_ASSERT(ir_kernel_while_loop_block);
        auto ir_while_builder = lowering::IrBuilder::Create();
        ir_while_builder->PushBlock(ir_kernel_while_loop_block);
        // 末尾应该有个Label, 故在开始插入
        ir_while_builder->inserter_iterator = ir_kernel_while_loop_block->begin();
        for (auto ir_value : *ir_gpu_for->LoopBlock()) {
            ir_while_builder->Insert(ir_value);
        }
    }

    ir_builder->Create<ir::Return>(ir_builder->Create<ir::VoidValue>());

    return std::make_tuple(ir_kernel_function, ir_captured_variables_list);
}

inline void ExtractGpuFor(std::shared_ptr<ir::Module> ir_module) {
    auto ir_gpu_fors = utility::GetAll<ir::For>(ir_module);
    ir_gpu_fors.remove_if([](std::shared_ptr<ir::For> ir_gpu_for) {
        return !ir_gpu_for->annotation_dict.count("gpu");
    });

    int64_t idx = 0;
    for (auto ir_gpu_for : ir_gpu_fors) {
        auto [ir_kernel_function, ir_captured_variables_list] =
            ConvertGpuForToKernelCall(ir_gpu_for, idx);

        auto ir_gpu_parent_block = ir_gpu_for->GetParentBlock();
        auto iter_gpu_for = std::find(RANGE((*ir_gpu_parent_block)), ir_gpu_for);
        auto ir_builder = lowering::IrBuilder::Create();
        ir_builder->PushBlock(ir_gpu_parent_block);
        ir_builder->inserter_iterator = iter_gpu_for;
        ir_builder->symbol_table = ir_module->symbol_table;

        //
        auto symbol_table_gpu =
            lowering::SymbolGet<lowering::SymbolTable>(ir_module->symbol_table->Get("gpu"));
        PRAJNA_ASSERT(symbol_table_gpu);
        auto ir_max_thread_per_block_function =
            lowering::SymbolGet<ir::Value>(symbol_table_gpu->Get("MaxThreadPerBlock"));
        PRAJNA_ASSERT(ir_max_thread_per_block_function);
        auto ir_multi_processor_count_function =
            lowering::SymbolGet<ir::Value>(symbol_table_gpu->Get("MultiProcessorsCount"));
        PRAJNA_ASSERT(ir_multi_processor_count_function);

        auto ir_zero = ir_builder->GetInt64Constant(0);
        auto ir_max_thread_per_block = ir_builder->Create<ir::Call>(
            ir_max_thread_per_block_function, std::list<std::shared_ptr<ir::Value>>{ir_zero});
        auto ir_multi_process_count = ir_builder->Create<ir::Call>(
            ir_multi_processor_count_function, std::list<std::shared_ptr<ir::Value>>{ir_zero});

        auto ir_grid_shape = ir_builder->Create<ir::LocalVariable>(ir_builder->GetShape3Type());
        ir_builder->SetDim3(ir_grid_shape, 0, ir_multi_process_count);
        ir_builder->SetDim3(ir_grid_shape, 1, ir_builder->GetInt64Constant(1));
        ir_builder->SetDim3(ir_grid_shape, 2, ir_builder->GetInt64Constant(1));

        auto ir_block_shape = ir_builder->Create<ir::LocalVariable>(ir_builder->GetShape3Type());
        ir_builder->SetDim3(ir_block_shape, 0, ir_max_thread_per_block);
        ir_builder->SetDim3(ir_block_shape, 1, ir_builder->GetInt64Constant(1));
        ir_builder->SetDim3(ir_block_shape, 2, ir_builder->GetInt64Constant(1));

        std::list<std::shared_ptr<ir::Value>> ir_arguments;
        ir_arguments.push_back(ir_gpu_for->First());
        ir_arguments.push_back(ir_gpu_for->Last());
        std::unordered_map<std::shared_ptr<ir::Value>, std::shared_ptr<ir::Variable>>
            gpu_host_tensor_dict;
        std::transform(
            RANGE(ir_captured_variables_list), std::back_inserter(ir_arguments),
            [=, &gpu_host_tensor_dict](
                std::shared_ptr<ir::Variable> ir_captured_variable) -> std::shared_ptr<ir::Value> {
                if (utility::IsHostTensorType(ir_captured_variable->type, ir_module)) {
                    auto ir_gpu_tensor =
                        ir_builder->CallMemberFunction(ir_captured_variable, "ToGpu", {});
                    gpu_host_tensor_dict[ir_gpu_tensor] = ir_captured_variable;
                    return ir_gpu_tensor;
                } else {
                    return ir_captured_variable;
                }
            });

        ir_builder->Create<ir::KernelFunctionCall>(ir_kernel_function, ir_grid_shape,
                                                   ir_block_shape, ir_arguments);
        for (auto [ir_gpu_tensor, ir_host_tensor_variable] : gpu_host_tensor_dict) {
            PRAJNA_ASSERT(utility::IsGpuTensorType(ir_gpu_tensor->type, ir_module));
            auto ir_host_tensor = ir_builder->CallMemberFunction(ir_gpu_tensor, "ToHost", {});
            ir_builder->Create<ir::WriteVariableLiked>(ir_host_tensor, ir_host_tensor_variable);
        }

        utility::RemoveFromParent(ir_gpu_for);
        ir_gpu_for->Finalize();
        ++idx;
    }
}

}  // namespace prajna::transform
