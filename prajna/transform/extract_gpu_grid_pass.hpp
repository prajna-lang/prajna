#pragma once

#include <boost/range/combine.hpp>

#include "prajna/ir/ir.hpp"
#include "prajna/logger.hpp"
#include "prajna/lowering/statement_lowering_visitor.hpp"
#include "prajna/parser/parse.h"
#include "prajna/transform/transform_pass.hpp"
#include "prajna/transform/utility.hpp"

namespace prajna::transform {

class ExtractGpuGridPass : public FunctionPass {
   public:
     std::shared_ptr<ir::Function> createKernelFunction(
         std::shared_ptr<ir::For> ir_for,
         std::vector<std::shared_ptr<ir::LocalVariable>> ir_variables,
         std::string ir_kernel_function_name) {
       std::vector<std::shared_ptr<ir::Type>> ir_argument_types(
           ir_variables.size());
       std::transform(
           RANGE(ir_variables), ir_argument_types.begin(),
           [](std::shared_ptr<ir::Value> ir_value) { return ir_value->type; });

       auto ir_host_function = ir_for->getParentFunction();
       auto ir_host_module = ir_host_function->parent_module;

       auto ir_kernel_function_type =
           ir::FunctionType::create(ir::VoidType::create(), ir_argument_types);
       auto ir_kernel_function = ir::Function::create(ir_kernel_function_type);
       if (!ir_host_module->modules[ir::Target::nvptx]) {
         ir_host_module->modules[ir::Target::nvptx] = ir::Module::create();
         ir_host_module->modules[ir::Target::nvptx]->parent_module =
             ir_host_module;
       }
       auto ir_nvptx_module = ir_host_module->modules[ir::Target::nvptx];

       ir_nvptx_module->functions.push_back(ir_kernel_function);
       //
       ir_kernel_function->name = ir_kernel_function_name;
       ir_kernel_function->fullname =
           concatFullname(ir_host_function->fullname, ir_kernel_function->name);
       ir_kernel_function->parent_module = ir_nvptx_module;
       ir_kernel_function->function_type->annotations["target"].push_back(
           "nvptx");
       ir_kernel_function->function_type->annotations.insert({"kernel", {}});

       auto ir_grid_block = ir_for->loopBlock();
       ir_grid_block->parent_block = nullptr;
       ir_kernel_function->arguments.resize(
           ir_kernel_function_type->argument_types.size());
       for (size_t i = 0; i < ir_kernel_function_type->argument_types.size();
            ++i) {
         ir_kernel_function->arguments[i] =
             ir::Argument::create(ir_argument_types[i]);
         ir_kernel_function->arguments[i]->parent_block = ir_grid_block;
       }

       // PRAJNA_ASSERT(ir_kernel_function->arguments.size() >= 2);
       // auto iter_arguments = ir_kernel_function->arguments.rbegin();
       // auto ir_grid_last = *iter_arguments;
       // ++iter_arguments;
       // auto ir_grid_first = *iter_arguments;

       //
       auto ir_top_block = ir::Block::create();
       ir_top_block->parent_function = ir_kernel_function;
       ir_kernel_function->blocks.push_back(ir_top_block);
       auto iter = ir_top_block->values.end();

       {
         std::string code = "var threadIdx_x = threadIdxX();";
         auto logger = Logger::create(code, CH);
         auto symbol_table =
             ir_for->getParentFunction()->parent_module->symbol_table;
         auto statement_lowering_visitor =
             lowering::StatementLoweringVisitor::create(symbol_table, logger);
         auto ir_utility = statement_lowering_visitor->ir_utility;
         ir_utility->pushSymbolTable();
         ir_utility->pushBlock(ir_top_block);
         // ir_utility->symbol_table->set(ir_tid_x_fun, "getThreadIdxX");

         auto ast = prajna::parser::parse(code, "//None", logger);

         statement_lowering_visitor->apply(ast);

         ir_utility->create<ir::Return>(ir::VoidValue::create());
         ir_utility->popSymbolTable();
         ir_utility->popBlock(ir_top_block);
       }

       return ir_kernel_function;

       // // 将捕获的变量替换为核函数的参数
       // for (auto [ir_variable, ir_argument] :
       //      boost::combine(ir_variables, ir_kernel_function->arguments)) { //
       //      combine会使用最短的size
       //     auto instruction_with_index_list =
       //     ir_variable->instruction_with_index_list; for (auto
       //     [ir_instruction, idx] : instruction_with_index_list) {
       //         //  只处理相关的操作, ir_grid外的不处理
       //         if (ir_instruction->parent_block == ir_grid_block) {
       //             ir_instruction->operand(ir_argument, idx);
       //         }
       //     }
       // }
     }

     void conveterGridToGpu(std::shared_ptr<ir::For> ir_for, size_t idx) {
       auto ir_captured_variable_vector =
           utility::captureExternalVariablesInBlock(ir_for->loopBlock());
       // 将index移除, 其会转换为核函数的内部参数
       ir_captured_variable_vector.remove(ir_for->index());
       utility::removeFromParent(ir_for->index());
       // 都是可捕获类型
       if (std::any_of(RANGE(ir_captured_variable_vector),
                       [](auto ir_variable) {
                         return !utility::isCapturedType(ir_variable->type);
                       })) {
         PRAJNA_TODO;
       }
       utility::eachValue(ir_for->loopBlock(), [&](auto ir_value) {
         // 不能被修改
         if (auto ir_write_variable_liked =
                 cast<ir::WriteVariableLiked>(ir_value)) {
           if (std::any_of(RANGE(ir_captured_variable_vector),
                           [&](auto ir_captured_variable) {
                             return utility::isReferedTo(
                                 ir_captured_variable,
                                 ir_write_variable_liked->variable());
                           })) {
             PRAJNA_TODO;
           }
         }
         // 不能获取地址, 因为是通过值拷贝捕获的
         if (auto ir_get_address_of =
                 cast<ir::GetAddressOfVariableLiked>(ir_value)) {
           if (std::any_of(RANGE(ir_captured_variable_vector),
                           [&](auto ir_captured_variable) {
                             return ir_captured_variable ==
                                    ir_get_address_of->variable();
                           })) {
             PRAJNA_TODO;
           }
         }
       });

       // 参数需要有顺序
       std::vector<std::shared_ptr<ir::LocalVariable>> ir_captured_variable_vec;
       auto ir_kernel_function =
           this->createKernelFunction(ir_for, ir_captured_variable_vec,
                                      "grid_kernel_" + std::to_string(idx));
       // 核函数并不会在这里生成, 起会在JIT的时候生成PTX,
       // 所以我们在这里用全局变量来表示它, 回头生产了再赋予全局变量,
       // 如果这里立即生成, 则代码的模块性会大受影响=\-
       auto ir_global_variable = ir::GlobalVariable::create(
           ir::PointerType::create(ir::IntType::create(64, 32)));
       ir_global_variable->parent_module =
           ir_for->getParentFunction()->parent_module;
       ir_global_variable->fullname = ir_kernel_function->fullname + "_pointer";
       // 调用核函数
       // lauchKernelFunction(ir_global_variable, gridX, )
       // cuda::cu

       utility::removeFromParent(ir_for);
     }

    bool runOnFunction(std::shared_ptr<ir::Function> ir_kernel_function) override {
        bool changed = true;

        auto ir_grids =
            utility::getValuesInFunction<ir::For>(ir_kernel_function);

        size_t i = 0;
        for (auto ir_for : ir_grids) {
          this->conveterGridToGpu(ir_for, i++);
        }

        return changed;
    }
};

}  // namespace prajna::transform
