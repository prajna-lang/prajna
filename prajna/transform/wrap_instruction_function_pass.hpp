#pragma once

#include "prajna/ir/ir.hpp"
#include "prajna/ir/operation_instruction.hpp"
#include "prajna/logger.hpp"
#include "prajna/transform/utility.hpp"

namespace prajna::transform {

inline std::shared_ptr<ir::Module> wrapInstructionFunction(std::shared_ptr<ir::Module> ir_module) {
    for (auto ir_function : ir_module->functions) {
        if (ir_function->isInstruction()) {
            auto ir_builder = lowering::IrBuilder::create();
            ir_builder->createTopBlockForFunction(ir_function);

            auto annotations_instruction = ir_function->annotation_dict["instruction"];
            PRAJNA_ASSERT(annotations_instruction.size() > 0);
            auto annotation_operation = *std::next(annotations_instruction.begin());

            auto ir_lhs_type = cast<ir::PointerType>(ir_function->parameters.front()->type);
            auto category = annotations_instruction.front();
            if (category == "CastInst") {
                PRAJNA_ASSERT(ir_function->parameters.size() == 1);
                PRAJNA_ASSERT(annotations_instruction.size() == 3);

                std::unordered_map<std::string, ir::CastInstruction::Operation>
                    cast_operation_dict = {
                        {"Trunc", ir::CastInstruction::Operation::Trunc},
                        {"ZExt", ir::CastInstruction::Operation::ZExt},
                        {"SExt", ir::CastInstruction::Operation::SExt},
                        {"FPToUI", ir::CastInstruction::Operation::FPToUI},
                        {"FPToSI", ir::CastInstruction::Operation::FPToSI},
                        {"UIToFP", ir::CastInstruction::Operation::UIToFP},
                        {"SIToFP", ir::CastInstruction::Operation::SIToFP},
                        {"FPTrunc", ir::CastInstruction::Operation::FPTrunc},
                        {"FPExt", ir::CastInstruction::Operation::FPExt},
                        {"PtrToInt", ir::CastInstruction::Operation::PtrToInt},
                        {"IntToPtr", ir::CastInstruction::Operation::IntToPtr},
                        {"BitCast", ir::CastInstruction::Operation::BitCast},
                        {"AddrSpaceCast", ir::CastInstruction::Operation::AddrSpaceCast},
                    };
                PRAJNA_ASSERT(cast_operation_dict.count(annotation_operation));
                auto cast_operation = cast_operation_dict[annotation_operation];

                auto ir_value =
                    ir_builder->create<ir::LoadPointer>(ir_function->parameters.front());
                auto ir_cast_instruction = ir_builder->create<ir::CastInstruction>(
                    cast_operation, ir_value, ir_function->function_type->return_type);
                ir_builder->create<ir::Return>(ir_cast_instruction);
                // 可以移除instruction的注解, 已经添加相应指令了
                ir_function->annotation_dict.erase("instruction");
                ir_function->is_declaration = false;

                continue;
            }

            if (category == "ICmp" || category == "FCmp") {
                PRAJNA_ASSERT(annotations_instruction.size() == 2);
                PRAJNA_ASSERT(ir_function->parameters.size() == 2);
                auto ir_lhs_type = cast<ir::PointerType>(ir_function->parameters.front()->type);

                std::unordered_map<std::string, ir::CompareInstruction::Operation>
                    compare_operation_dict = {
                        {"FCMP_FALSE", ir::CompareInstruction::Operation::FCMP_FALSE},
                        {"FCMP_OEQ", ir::CompareInstruction::Operation::FCMP_OEQ},
                        {"FCMP_OGT", ir::CompareInstruction::Operation::FCMP_OGT},
                        {"FCMP_OGE", ir::CompareInstruction::Operation::FCMP_OGE},
                        {"FCMP_OLT", ir::CompareInstruction::Operation::FCMP_OLT},
                        {"FCMP_OLE", ir::CompareInstruction::Operation::FCMP_OLE},
                        {"FCMP_ONE", ir::CompareInstruction::Operation::FCMP_ONE},
                        {"FCMP_ORD", ir::CompareInstruction::Operation::FCMP_ORD},
                        {"FCMP_UNO", ir::CompareInstruction::Operation::FCMP_UNO},
                        {"FCMP_UEQ", ir::CompareInstruction::Operation::FCMP_UEQ},
                        {"FCMP_UGT", ir::CompareInstruction::Operation::FCMP_UGT},
                        {"FCMP_UGE", ir::CompareInstruction::Operation::FCMP_UGE},
                        {"FCMP_ULT", ir::CompareInstruction::Operation::FCMP_ULT},
                        {"FCMP_ULE", ir::CompareInstruction::Operation::FCMP_ULE},
                        {"FCMP_UNE", ir::CompareInstruction::Operation::FCMP_UNE},
                        {"FCMP_TRUE", ir::CompareInstruction::Operation::FCMP_TRUE},
                        {"ICMP_EQ", ir::CompareInstruction::Operation::ICMP_EQ},
                        {"ICMP_NE", ir::CompareInstruction::Operation::ICMP_NE},
                        {"ICMP_UGT", ir::CompareInstruction::Operation::ICMP_UGT},
                        {"ICMP_UGE", ir::CompareInstruction::Operation::ICMP_UGE},
                        {"ICMP_ULT", ir::CompareInstruction::Operation::ICMP_ULT},
                        {"ICMP_ULE", ir::CompareInstruction::Operation::ICMP_ULE},
                        {"ICMP_SGT", ir::CompareInstruction::Operation::ICMP_SGT},
                        {"ICMP_SGE", ir::CompareInstruction::Operation::ICMP_SGE},
                        {"ICMP_SLT", ir::CompareInstruction::Operation::ICMP_SLT},
                        {"ICMP_SLE", ir::CompareInstruction::Operation::ICMP_SLE},
                    };

                PRAJNA_ASSERT(compare_operation_dict.count(annotation_operation));
                auto compare_operation = compare_operation_dict[annotation_operation];

                auto ir_operand0 =
                    ir_builder->create<ir::LoadPointer>(ir_function->parameters.front());
                auto ir_operand1 = ir_function->parameters.back();
                auto ir_compare_instruction = ir_builder->create<ir::CompareInstruction>(
                    compare_operation, ir_operand0, ir_operand1);
                ir_builder->create<ir::Return>(ir_compare_instruction);
                // 可以移除instruction的注解, 已经添加相应指令了
                ir_function->annotation_dict.erase("instruction");
                ir_function->is_declaration = false;

                continue;
            }

            if (category == "BinaryOperator") {
                PRAJNA_ASSERT(annotations_instruction.size() == 2);
                PRAJNA_ASSERT(ir_function->parameters.size() == 2);

                std::unordered_map<std::string, ir::BinaryOperator::Operation>
                    binary_operation_dict = {
                        {"Add", ir::BinaryOperator::Operation::Add},
                        {"Sub", ir::BinaryOperator::Operation::Sub},
                        {"Mul", ir::BinaryOperator::Operation::Mul},
                        {"SDiv", ir::BinaryOperator::Operation::SDiv},
                        {"UDiv", ir::BinaryOperator::Operation::UDiv},
                        {"SRem", ir::BinaryOperator::Operation::SRem},
                        {"URem", ir::BinaryOperator::Operation::URem},
                        {"And", ir::BinaryOperator::Operation::And},
                        {"Or", ir::BinaryOperator::Operation::Or},
                        {"Shl", ir::BinaryOperator::Operation::Shl},
                        {"LShr", ir::BinaryOperator::Operation::LShr},
                        {"AShr", ir::BinaryOperator::Operation::AShr},
                        {"Xor", ir::BinaryOperator::Operation::Xor},
                        {"FAdd", ir::BinaryOperator::Operation::FAdd},
                        {"FSub", ir::BinaryOperator::Operation::FSub},
                        {"FMul", ir::BinaryOperator::Operation::FMul},
                        {"FDiv", ir::BinaryOperator::Operation::FDiv},
                        {"FRem", ir::BinaryOperator::Operation::FRem},
                    };
                PRAJNA_ASSERT(binary_operation_dict.count(annotation_operation));
                auto binary_operation = binary_operation_dict[annotation_operation];

                auto ir_operand0 =
                    ir_builder->create<ir::LoadPointer>(ir_function->parameters.front());
                auto ir_operand1 = ir_function->parameters.back();
                auto ir_binary_operator_instruction = ir_builder->create<ir::BinaryOperator>(
                    binary_operation, ir_operand0, ir_operand1);
                ir_builder->create<ir::Return>(ir_binary_operator_instruction);
                // 可以移除instruction的注解, 已经添加相应指令了
                ir_function->annotation_dict.erase("instruction");
                ir_function->is_declaration = false;

                continue;
            }
        }
    }

    for (auto [ir_target, ir_sub_module] : ir_module->modules) {
        if (not ir_module) continue;

        wrapInstructionFunction(ir_sub_module);
    }

    return ir_module;
}

}  // namespace prajna::transform
