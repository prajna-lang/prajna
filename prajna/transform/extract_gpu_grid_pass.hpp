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

inline auto convertGpuForToKernelCall(std::shared_ptr<ir::For> ir_gpu_for, size_t idx) {
    auto ir_module = ir_gpu_for->getParentFunction()->parent_module;
    auto ir_nvptx_module = ir_module->modules[ir::Target::nvptx];

    auto ir_captured_variables_list =
        utility::captureExternalVariablesInBlock(ir_gpu_for->loopBlock());
    // 将index, first, last
    ir_captured_variables_list.remove(ir_gpu_for->index());
    // todo
    utility::removeFromParent(ir_gpu_for->index());

    std::vector<std::shared_ptr<ir::Type>> ir_argument_types(2);
    // 加入first和last
    ir_argument_types[0] = ir_gpu_for->first()->type;
    ir_argument_types[1] = ir_gpu_for->last()->type;
    std::transform(RANGE(ir_captured_variables_list), std::back_inserter(ir_argument_types),
                   [](std::shared_ptr<ir::Value> ir_value) {
                       if (utility::isHostTensorType(ir_value->type)) {
                           return utility::getGpuTensorTypeOfHostTensorType(ir_value->type);
                       } else {
                           return ir_value->type;
                       }
                   });

    auto ir_kernel_function_type =
        ir::FunctionType::create(ir_argument_types, ir::VoidType::create());
    auto ir_kernel_function = ir::Function::create(ir_kernel_function_type);
    ir_kernel_function->fullname = concatFullname(ir_gpu_for->getParentFunction()->fullname,
                                                  "auto_created_gpu_for_" + std::to_string(idx));
    ir_kernel_function->name = ir_kernel_function->fullname;
    ir_kernel_function_type->fullname = ir_kernel_function->fullname;
    ir_kernel_function_type->name = ir_kernel_function->fullname;
    ir_kernel_function->parent_module = ir_nvptx_module;
    ir_kernel_function->annotations["target"].push_back("nvptx");
    ir_kernel_function->annotations.insert({"kernel", {}});
    ir_nvptx_module->functions.push_back(ir_kernel_function);

    auto ir_block = ir::Block::create();
    ir_block->parent_function = ir_kernel_function;
    ir_kernel_function->blocks.push_back(ir_block);

    auto ir_builder = lowering::IrBuilder::create();
    ir_builder->pushBlock(ir_block);
    ir_builder->inserter_iterator = ir_block->values.end();

    std::unordered_map<std::shared_ptr<ir::Value>, std::shared_ptr<ir::Value>> variables_dict;
    auto iter_captured_variable = ir_captured_variables_list.begin();
    auto ir_first = ir_kernel_function->parameters[0];
    variables_dict[ir_gpu_for->first()] = ir_first;
    ir_kernel_function->parameters[0] = ir_first;
    auto ir_last = ir_kernel_function->parameters[1];
    variables_dict[ir_gpu_for->last()] = ir_last;
    ir_kernel_function->parameters[1] = ir_last;
    for (size_t i = 2; i < ir_argument_types.size(); ++i, ++iter_captured_variable) {
        auto ir_argument = ir_kernel_function->parameters[i];
        // 需要搞成临时变量, 这样才会原来的变量对应, 以便访问成员函数等
        variables_dict[*iter_captured_variable] = ir_builder->variableLikedNormalize(ir_argument);
    }

    // 将捕获的变量置换为参数对应的临时变量
    for (auto ir_captured_variable : ir_captured_variables_list) {
        auto instruction_with_index_inner_list = ir_captured_variable->instruction_with_index_list;
        instruction_with_index_inner_list.remove_if(
            [=](ir::InstructionWithOperandIndex instruction_with_operand_index) -> bool {
                return instruction_with_operand_index.instruction->getRootBlock() !=
                       ir_gpu_for->loopBlock();
            });
        for (auto instruction_with_operand_index : instruction_with_index_inner_list) {
            auto ir_instruction = instruction_with_operand_index.instruction;
            auto operand_index = instruction_with_operand_index.operand_index;
            PRAJNA_ASSERT(variables_dict.count(ir_captured_variable));
            ir_instruction->operand(operand_index, variables_dict[ir_captured_variable]);
        }
    }

    auto ir_index = ir_builder->create<ir::LocalVariable>(ir_builder->getIndexType());
    ir_index->name = "i";
    auto ir_gpu_for_index_inst_with_idx_list_copy =
        ir_gpu_for->index()->instruction_with_index_list;
    for (auto inst_with_idx : ir_gpu_for_index_inst_with_idx_list_copy) {
        auto ir_instruction = inst_with_idx.instruction;
        // ir_gpu_for将被移除, 不能再使用ir_index
        if (ir_instruction == ir_gpu_for) continue;

        auto operand_index = inst_with_idx.operand_index;
        ir_instruction->operand(operand_index, ir_index);
    }

    {
        std::string code =
            "var thread_idx = gpu::threadIndex();"
            "var block_idx = gpu::blockIndex();"
            "var block_shape = gpu::blockShape();"
            "var grid_shape = gpu::gridShape();"
            "i = first + thread_idx[0] + block_idx[0] * block_shape[0];"
            "while (i < last){"
            "   i = i + block_shape[0] * grid_shape[0];"
            "}";

        auto logger = Logger::create(code);
        auto symbol_table = ir_module->symbol_table;
        auto statement_lowering_visitor =
            lowering::StatementLoweringVisitor::create(symbol_table, logger, nullptr, nullptr);
        auto ir_builder = statement_lowering_visitor->ir_builder;
        ir_builder->pushSymbolTable();
        ir_builder->pushBlock(ir_block);

        ir_builder->symbol_table->set(ir_index, "i");
        ir_builder->symbol_table->set(ir_first, "first");
        ir_builder->symbol_table->set(ir_last, "last");

        auto ast = prajna::parser::parse(code, "//None", logger);

        statement_lowering_visitor->apply(ast);
        ir_builder->popSymbolTable();
        ir_builder->popBlock();

        // 插入ir_gpu_for里的逻辑
        auto ir_kernel_while = cast<ir::While>(*std::prev(ir_block->values.end(), 2));
        PRAJNA_ASSERT(ir_kernel_while);
        auto ir_kernel_while_loop_block =
            cast<ir::Block>(ir_kernel_while->loopBlock()->values.front());
        PRAJNA_ASSERT(ir_kernel_while_loop_block);
        auto ir_while_builder = lowering::IrBuilder::create();
        ir_while_builder->pushBlock(ir_kernel_while_loop_block);
        // 末尾应该有个Label, 故在开始插入
        ir_while_builder->inserter_iterator = ir_kernel_while_loop_block->values.begin();
        for (auto ir_value : ir_gpu_for->loopBlock()->values) {
            ir_while_builder->insert(ir_value);
        }
    }

    ir_builder->create<ir::Return>(ir_builder->create<ir::VoidValue>());

    return std::make_tuple(ir_kernel_function, ir_captured_variables_list);
}

inline std::shared_ptr<ir::Module> extractGpuFor(std::shared_ptr<ir::Module> ir_module) {
    auto ir_gpu_fors = utility::getValuesInModule<ir::For>(ir_module);
    ir_gpu_fors.remove_if([](std::shared_ptr<ir::For> ir_gpu_for) {
        return not ir_gpu_for->annotations.count("gpu");
    });

    size_t idx = 0;
    for (auto ir_gpu_for : ir_gpu_fors) {
        auto [ir_kernel_function, ir_captured_variables_list] =
            convertGpuForToKernelCall(ir_gpu_for, idx);

        auto ir_gpu_parent_block = ir_gpu_for->parent_block;
        auto iter_gpu_for = std::find(RANGE(ir_gpu_parent_block->values), ir_gpu_for);
        auto ir_builder = lowering::IrBuilder::create();
        ir_builder->pushBlock(ir_gpu_parent_block);
        ir_builder->inserter_iterator = iter_gpu_for;
        ir_builder->symbol_table = ir_module->symbol_table;

        //
        auto symbol_table_gpu =
            lowering::symbolGet<lowering::SymbolTable>(ir_module->symbol_table->get("gpu"));
        PRAJNA_ASSERT(symbol_table_gpu);
        auto ir_max_thread_per_block_function =
            lowering::symbolGet<ir::Value>(symbol_table_gpu->get("maxThreadPerBlock"));
        PRAJNA_ASSERT(ir_max_thread_per_block_function);
        auto ir_multi_processor_count_function =
            lowering::symbolGet<ir::Value>(symbol_table_gpu->get("multiProcessorsCount"));
        PRAJNA_ASSERT(ir_multi_processor_count_function);

        auto ir_zero = ir_builder->getIndexConstant(0);
        auto ir_max_thread_per_block = ir_builder->create<ir::Call>(
            ir_max_thread_per_block_function, std::vector<std::shared_ptr<ir::Value>>{ir_zero});
        auto ir_multi_process_count = ir_builder->create<ir::Call>(
            ir_multi_processor_count_function, std::vector<std::shared_ptr<ir::Value>>{ir_zero});

        auto ir_grid_shape = ir_builder->create<ir::LocalVariable>(ir_builder->getShape3Type());
        ir_builder->setDim3(ir_grid_shape, 0, ir_multi_process_count);
        ir_builder->setDim3(ir_grid_shape, 1, ir_builder->getIndexConstant(1));
        ir_builder->setDim3(ir_grid_shape, 2, ir_builder->getIndexConstant(1));

        auto ir_block_shape = ir_builder->create<ir::LocalVariable>(ir_builder->getShape3Type());
        ir_builder->setDim3(ir_block_shape, 0, ir_max_thread_per_block);
        ir_builder->setDim3(ir_block_shape, 1, ir_builder->getIndexConstant(1));
        ir_builder->setDim3(ir_block_shape, 2, ir_builder->getIndexConstant(1));

        std::vector<std::shared_ptr<ir::Value>> ir_arguments;
        ir_arguments.push_back(ir_gpu_for->first());
        ir_arguments.push_back(ir_gpu_for->last());
        std::unordered_map<std::shared_ptr<ir::Value>, std::shared_ptr<ir::Variable>>
            gpu_host_tensor_dict;
        std::transform(
            RANGE(ir_captured_variables_list), std::back_inserter(ir_arguments),
            [=, &gpu_host_tensor_dict](
                std::shared_ptr<ir::Variable> ir_captured_variable) -> std::shared_ptr<ir::Value> {
                if (utility::isHostTensorType(ir_captured_variable->type)) {
                    auto ir_gpu_tensor =
                        ir_builder->callMemberFunction(ir_captured_variable, "toGpu", {});
                    gpu_host_tensor_dict[ir_gpu_tensor] = ir_captured_variable;
                    return ir_gpu_tensor;
                } else {
                    return ir_captured_variable;
                }
            });

        ir_builder->create<ir::KernelFunctionCall>(ir_kernel_function, ir_grid_shape,
                                                   ir_block_shape, ir_arguments);
        for (auto [ir_gpu_tensor, ir_host_tensor_variable] : gpu_host_tensor_dict) {
            PRAJNA_ASSERT(utility::isGpuTensorType(ir_gpu_tensor->type));
            auto ir_host_tensor = ir_builder->callMemberFunction(ir_gpu_tensor, "toHost", {});
            ir_builder->create<ir::WriteVariableLiked>(ir_host_tensor, ir_host_tensor_variable);
        }

        utility::removeFromParent(ir_gpu_for);
        ir_gpu_for->finalize();
        ++idx;
    }

    return ir_module;
}

}  // namespace prajna::transform
