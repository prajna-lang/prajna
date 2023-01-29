#pragma once

#include "prajna/ir/value.hpp"

namespace prajna::ir {

class CompareInstruction : public Instruction {
   protected:
    CompareInstruction() = default;

   public:
    enum struct Operation {
        FCMP_FALSE,
        FCMP_OEQ,
        FCMP_OGT,
        FCMP_OGE,
        FCMP_OLT,
        FCMP_OLE,
        FCMP_ONE,
        FCMP_ORD,
        FCMP_UNO,
        FCMP_UEQ,
        FCMP_UGT,
        FCMP_UGE,
        FCMP_ULT,
        FCMP_ULE,
        FCMP_UNE,
        FCMP_TRUE,
        ICMP_EQ,
        ICMP_NE,
        ICMP_UGT,
        ICMP_UGE,
        ICMP_ULT,
        ICMP_ULE,
        ICMP_SGT,
        ICMP_SGE,
        ICMP_SLT,
        ICMP_SLE
    };

    static std::shared_ptr<CompareInstruction> create(Operation operation,
                                                      std::shared_ptr<ir::Value> ir_operand0,
                                                      std::shared_ptr<ir::Value> ir_operand1) {
        std::shared_ptr<CompareInstruction> self(new CompareInstruction);
        self->operation = operation;
        self->operandResize(2);
        self->operand(0, ir_operand0);
        self->operand(1, ir_operand1);
        self->type = ir::BoolType::create();
        self->tag = "CompareInstruction";
        return self;
    }

    Operation operation;
};

class BinaryOperator : public Instruction {
   protected:
    BinaryOperator() = default;

   public:
    enum struct Operation {
        Add,
        Sub,
        Mul,
        SDiv,
        UDiv,
        SRem,
        URem,
        And,
        Or,
        Shl,
        LShr,
        AShr,
        Xor,
        FAdd,
        FSub,
        FMul,
        FDiv,
        FRem
    };

    static std::shared_ptr<BinaryOperator> create(Operation operation,
                                                  std::shared_ptr<ir::Value> ir_operand0,
                                                  std::shared_ptr<ir::Value> ir_operand1) {
        std::shared_ptr<BinaryOperator> self(new BinaryOperator);
        PRAJNA_ASSERT(ir_operand0->type == ir_operand1->type);
        self->operation = operation;
        self->operandResize(2);
        self->operand(0, ir_operand0);
        self->operand(1, ir_operand1);
        self->type = ir_operand0->type;
        self->tag = "BinaryOperator";
        return self;
    }

    Operation operation;
};

class CastInstruction : public Instruction {
   protected:
    CastInstruction() = default;

   public:
    enum struct Operation {
        Trunc,
        ZExt,
        SExt,
        FPToUI,
        FPToSI,
        UIToFP,
        SIToFP,
        FPTrunc,
        FPExt,
        PtrToInt,
        IntToPtr,
        BitCast,
        AddrSpaceCast
    };

    static std::shared_ptr<CastInstruction> create(Operation operation,
                                                   std::shared_ptr<Value> ir_operand,
                                                   std::shared_ptr<Type> ir_type) {
        std::shared_ptr<CastInstruction> self(new CastInstruction);
        self->operation = operation;
        self->operandResize(1);
        self->operand(0, ir_operand);
        self->type = ir_type;
        self->tag = "CastInstruction";
        return self;
    };

   public:
    Operation operation;
};

}  // namespace prajna::ir
