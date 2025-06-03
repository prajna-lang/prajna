#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "prajna/ir/value.hpp"

namespace prajna::ir {

class IRPrinter : public Visitor {
   protected:
    IRPrinter() = default;

   public:
    static std::shared_ptr<IRPrinter> Create() {
        auto self = std::shared_ptr<IRPrinter>(new IRPrinter);
        return self;
    }

    std::string Print(std::shared_ptr<Value> ir_value) {
        var_counter = 0;
        var_name_dict.clear();  // 清空变量名映射
        output.str("");         // 清空输出
        ir_value->ApplyVisitor(this->shared_from_this());
        return output.str();
    }

    void Dump(std::shared_ptr<Value> ir_value) {
        auto str = this->Print(ir_value);
        std::cout << str;
    }

    void Visit(std::shared_ptr<VoidValue> ir_void_value) override {
        output << GetVariableName(ir_void_value) << " = void;\n";
    }

    void Visit(std::shared_ptr<Parameter> ir_parameter) override {
        output << GetVariableName(ir_parameter) << " = parameter "
               << TypeToString(ir_parameter->type);
        if (ir_parameter->no_alias) output << " noalias";
        if (ir_parameter->no_capture) output << " nocapture";
        if (ir_parameter->no_undef) output << " noundef";
        if (ir_parameter->readonly) output << " readonly";
        output << ";\n";
    }

    void Visit(std::shared_ptr<ConstantBool> ir_constant_bool) override {
        output << GetVariableName(ir_constant_bool) << " = " << TypeToString(ir_constant_bool->type)
               << " " << ir_constant_bool->value << ";\n";
    }

    void Visit(std::shared_ptr<ConstantFloat> ir_constant_float) override {
        output << GetVariableName(ir_constant_float) << " = "
               << TypeToString(ir_constant_float->type) << " ";
        if (ir_constant_float->special_value == ConstantFloat::SpecialValue::NaN) {
            output << "nan";
        } else if (ir_constant_float->special_value == ConstantFloat::SpecialValue::Inf) {
            output << (ir_constant_float->is_negative ? "-inf" : "inf");
        } else if (ir_constant_float->special_value == ConstantFloat::SpecialValue::Smallest) {
            output << "smallest";
        } else if (ir_constant_float->special_value == ConstantFloat::SpecialValue::Largest) {
            output << "largest";
        } else {
            output << (ir_constant_float->is_negative ? "-" : "") << ir_constant_float->value;
        }
        output << ";\n";
    }

    void Visit(std::shared_ptr<ConstantChar> ir_constant_char) override {
        output << GetVariableName(ir_constant_char) << " = " << TypeToString(ir_constant_char->type)
               << " '" << ir_constant_char->value << "';\n";
    }

    void Visit(std::shared_ptr<ConstantArray> ir_constant_array) override {
        output << GetVariableName(ir_constant_array) << " = "
               << TypeToString(ir_constant_array->type) << " [";
        bool first = true;
        for (auto ir_constant : ir_constant_array->initialize_constants) {
            if (!first) output << ", ";
            output << GetVariableName(ir_constant);
            first = false;
        }
        output << "];\n";
    }

    void Visit(std::shared_ptr<ConstantVector> ir_constant_vector) override {
        output << GetVariableName(ir_constant_vector) << " = "
               << TypeToString(ir_constant_vector->type) << " <";
        bool first = true;
        for (auto ir_constant : ir_constant_vector->initialize_constants) {
            if (!first) output << ", ";
            output << GetVariableName(ir_constant);
            first = false;
        }

        output << ">;\n";
    }

    void Visit(std::shared_ptr<MemberFunctionWithThisPointer> ir_member_function_with_this_pointer)
        override {
        output << GetVariableName(ir_member_function_with_this_pointer)
               << " = member_function_with_this_pointer "
               << GetVariableName(ir_member_function_with_this_pointer->this_pointer) << "."
               << ir_member_function_with_this_pointer->function_prototype->name << ";\n";
    }

    void Visit(std::shared_ptr<LocalVariable> ir_local_variable) override {
        output << GetVariableName(ir_local_variable) << " = LocalVariable " << ir_local_variable
               << ";\n";
    }

    void Visit(std::shared_ptr<AccessField> ir_access_field) override {
        output << GetVariableName(ir_access_field) << " = AccessField "
               << GetVariableName(ir_access_field->object()) << "." << ir_access_field->field->name
               << ";\n";
    }

    void Visit(std::shared_ptr<IndexArray> ir_index_array) override {
        output << GetVariableName(ir_index_array) << " = IndexArray "
               << GetVariableName(ir_index_array->object()) << "["
               << GetVariableName(ir_index_array->IndexVariable()) << "];\n";
    }

    void Visit(std::shared_ptr<IndexPointer> ir_index_pointer) override {
        output << GetVariableName(ir_index_pointer) << " = IndexPointer "
               << GetVariableName(ir_index_pointer->object()) << "["
               << GetVariableName(ir_index_pointer->IndexVariable()) << "];\n";
    }

    void Visit(std::shared_ptr<GetStructElementPointer> ir_get_struct_element_pointer) override {
        output << GetVariableName(ir_get_struct_element_pointer) << " = GetStructElementPointer "
               << GetVariableName(ir_get_struct_element_pointer->Pointer()) << "."
               << ir_get_struct_element_pointer->field->name << ";\n";
    }

    void Visit(std::shared_ptr<GetArrayElementPointer> ir_get_array_element_pointer) override {
        output << GetVariableName(ir_get_array_element_pointer) << " = GetArrayElementPointer "
               << GetVariableName(ir_get_array_element_pointer->Pointer()) << "["
               << GetVariableName(ir_get_array_element_pointer->IndexVariable()) << "];\n";
    }

    void Visit(std::shared_ptr<GetPointerElementPointer> ir_get_pointer_elelment_pointer) override {
        output << GetVariableName(ir_get_pointer_elelment_pointer) << " = GetPointerElementPointer "
               << GetVariableName(ir_get_pointer_elelment_pointer->Pointer()) << "["
               << GetVariableName(ir_get_pointer_elelment_pointer->IndexVariable()) << "];\n";
    }

    void Visit(std::shared_ptr<DeferencePointer> ir_deference_pointer) override {
        output << GetVariableName(ir_deference_pointer) << " = DeferencePointer "
               << GetVariableName(ir_deference_pointer->Pointer()) << ";\n";
    }

    void Visit(std::shared_ptr<WriteVariableLiked> ir_write_variable_liked) override {
        output << "WriteVariableLiked " << GetVariableName(ir_write_variable_liked->variable())
               << " " << GetVariableName(ir_write_variable_liked->Value()) << ";\n";
    }

    void Visit(
        std::shared_ptr<GetAddressOfVariableLiked> ir_get_address_of_variable_liked) override {
        output << GetVariableName(ir_get_address_of_variable_liked)
               << " = GetAddressOfVariableLiked "
               << GetVariableName(ir_get_address_of_variable_liked->variable()) << ";\n";
    }

    void Visit(std::shared_ptr<Alloca> ir_alloca) override {
        output << GetVariableName(ir_alloca) << " = Alloca " << TypeToString(ir_alloca->type);
        if (ir_alloca->Length()) {
            output << ", length " << GetVariableName(ir_alloca->Length());
        }
        if (ir_alloca->alignment != -1) {
            output << ", align " << ir_alloca->alignment;
        }
        output << ";\n";
    }

    void Visit(std::shared_ptr<GlobalAlloca> ir_global_alloca) override {
        output << GetVariableName(ir_global_alloca) << " = GlobalAlloca "
               << TypeToString(ir_global_alloca->type);
        if (ir_global_alloca->is_external) {
            output << " external";
        }
        if (ir_global_alloca->address_space != 0) {
            output << ", address_space " << ir_global_alloca->address_space;
        }
        output << ";\n";
    }

    void Visit(std::shared_ptr<LoadPointer> ir_load) override {
        output << GetVariableName(ir_load) << " = LoadPointer "
               << GetVariableName(ir_load->Pointer()) << ";\n";
    }

    void Visit(std::shared_ptr<StorePointer> ir_store) override {
        output << "StorePointer " << GetVariableName(ir_store->Value()) << ", "
               << GetVariableName(ir_store->Pointer()) << ";\n";
    }

    void Visit(std::shared_ptr<Return> ir_return) override {
        output << "Return " << GetVariableName(ir_return->Value()) << ";\n";
    }

    void Visit(std::shared_ptr<BitCast> ir_bitcast) override {
        output << GetVariableName(ir_bitcast) << " = BitCast " << TypeToString(ir_bitcast->type)
               << " " << GetVariableName(ir_bitcast->Value()) << ";\n";
    }

    void Visit(std::shared_ptr<Call> ir_call) override {
        output << "Call " << GetVariableName(ir_call) << " = Call  @" << ir_call->Function()->name
               << "(";
        for (int64_t i = 0; i < ir_call->ArgumentSize(); ++i) {
            if (i > 0) output << ", ";
            output << GetVariableName(ir_call->Argument(i));
        }
        output << ");\n";
    }

    void Visit(std::shared_ptr<ConditionBranch> ir_cond_branch) override {
        output << "ConditionBranch " << GetVariableName(ir_cond_branch->Condition()) << ", "
               << GetVariableName(ir_cond_branch->TrueBlock()) << ", "
               << GetVariableName(ir_cond_branch->FalseBlock()) << ";\n";
    }

    void Visit(std::shared_ptr<JumpBranch> ir_jump) override {
        output << "JumpBranch " << GetVariableName(ir_jump->NextBlock()) << ";\n";
    }

    void Visit(std::shared_ptr<Break> ir_break) override {
        output << "Break " << GetVariableName(ir_break->Loop()) << ";\n";
    }

    void Visit(std::shared_ptr<Continue> ir_continue) override {
        output << "Continue " << GetVariableName(ir_continue->Loop()) << ";\n";
    }

    void Visit(std::shared_ptr<GlobalVariable> ir_global_variable) override {
        output << GetVariableName(ir_global_variable) << " = GlobalVariable "
               << TypeToString(ir_global_variable->type);
        if (ir_global_variable->is_external) {
            output << " external";
        }
        output << ";\n";
    }

    void Visit(std::shared_ptr<AccessProperty> ir_access_property) override {
        output << GetVariableName(ir_access_property) << " = AccessProperty "
               << GetVariableName(ir_access_property->ThisPointer()) << ".";
        //    << ir_access_property->property->name;
        auto args = ir_access_property->Arguments();
        if (!args.empty()) {
            output << "(";
            bool first = true;
            for (auto arg : args) {
                if (!first) output << ", ";
                output << GetVariableName(arg);
                first = false;
            }
            output << ")";
        }
        output << ";\n";
    }
    void Visit(std::shared_ptr<WriteProperty> ir_write_property) override {
        output << "WriteProperty " << GetVariableName(ir_write_property->property()) << " = "
               << GetVariableName(ir_write_property->Value()) << ";\n";
    }

    void Visit(std::shared_ptr<ShuffleVector> ir_shuffle_vector) override {
        output << GetVariableName(ir_shuffle_vector) << " = ShuffleVector "
               << GetVariableName(ir_shuffle_vector->Value()) << ", "
               << GetVariableName(ir_shuffle_vector->Mask()) << ";\n";
    }

    void Visit(std::shared_ptr<CompareInstruction> ir_compare) override {
        output << GetVariableName(ir_compare) << " = ";
        switch (ir_compare->operation) {
            case CompareInstruction::Operation::ICMP_EQ:
                output << "icmp eq ";
                break;
            case CompareInstruction::Operation::ICMP_NE:
                output << "icmp ne ";
                break;
            case CompareInstruction::Operation::ICMP_UGT:
                output << "icmp ugt ";
                break;
            case CompareInstruction::Operation::ICMP_UGE:
                output << "icmp uge ";
                break;
            case CompareInstruction::Operation::ICMP_ULT:
                output << "icmp ult ";
                break;
            case CompareInstruction::Operation::ICMP_ULE:
                output << "icmp ule ";
                break;
            case CompareInstruction::Operation::ICMP_SGT:
                output << "icmp sgt ";
                break;
            case CompareInstruction::Operation::ICMP_SGE:
                output << "icmp sge ";
                break;
            case CompareInstruction::Operation::ICMP_SLT:
                output << "icmp slt ";
                break;
            case CompareInstruction::Operation::ICMP_SLE:
                output << "icmp sle ";
                break;
            case CompareInstruction::Operation::FCMP_OEQ:
                output << "fcmp oeq ";
                break;
            case CompareInstruction::Operation::FCMP_OGT:
                output << "fcmp ogt ";
                break;
            case CompareInstruction::Operation::FCMP_OGE:
                output << "fcmp oge ";
                break;
            case CompareInstruction::Operation::FCMP_OLT:
                output << "fcmp olt ";
                break;
            case CompareInstruction::Operation::FCMP_OLE:
                output << "fcmp ole ";
                break;
            case CompareInstruction::Operation::FCMP_ONE:
                output << "fcmp one ";
                break;
            case CompareInstruction::Operation::FCMP_ORD:
                output << "fcmp ord ";
                break;
            case CompareInstruction::Operation::FCMP_UNO:
                output << "fcmp uno ";
                break;
            case CompareInstruction::Operation::FCMP_UEQ:
                output << "fcmp ueq ";
                break;
            case CompareInstruction::Operation::FCMP_UGT:
                output << "fcmp ugt ";
                break;
            case CompareInstruction::Operation::FCMP_UGE:
                output << "fcmp uge ";
                break;
            case CompareInstruction::Operation::FCMP_ULT:
                output << "fcmp ult ";
                break;
            case CompareInstruction::Operation::FCMP_ULE:
                output << "fcmp ule ";
                break;
            case CompareInstruction::Operation::FCMP_UNE:
                output << "fcmp une ";
                break;
            case CompareInstruction::Operation::FCMP_TRUE:
                output << "fcmp true ";
                break;
            case CompareInstruction::Operation::FCMP_FALSE:
                output << "fcmp false ";
                break;
            default:
                output << "cmp unknown ";
                break;
        }
        output << TypeToString(ir_compare->GetOperand(0)->type) << " "
               << GetVariableName(ir_compare->GetOperand(0)) << ", "
               << GetVariableName(ir_compare->GetOperand(1)) << ";\n";
    }

    void Visit(std::shared_ptr<BinaryOperator> ir_binary_op) override {
        output << GetVariableName(ir_binary_op) << " = ";
        switch (ir_binary_op->operation) {
            case BinaryOperator::Operation::Add:
                output << "add ";
                break;
            case BinaryOperator::Operation::Sub:
                output << "sub ";
                break;
            case BinaryOperator::Operation::Mul:
                output << "mul ";
                break;
            case BinaryOperator::Operation::SDiv:
                output << "sdiv ";
                break;
            case BinaryOperator::Operation::UDiv:
                output << "udiv ";
                break;
            case BinaryOperator::Operation::SRem:
                output << "srem ";
                break;
            case BinaryOperator::Operation::URem:
                output << "urem ";
                break;
            case BinaryOperator::Operation::And:
                output << "and ";
                break;
            case BinaryOperator::Operation::Or:
                output << "or ";
                break;
            case BinaryOperator::Operation::Shl:
                output << "shl ";
                break;
            case BinaryOperator::Operation::LShr:
                output << "lshr ";
                break;
            case BinaryOperator::Operation::AShr:
                output << "ashr ";
                break;
            case BinaryOperator::Operation::Xor:
                output << "xor ";
                break;
            case BinaryOperator::Operation::FAdd:
                output << "fadd ";
                break;
            case BinaryOperator::Operation::FSub:
                output << "fsub ";
                break;
            case BinaryOperator::Operation::FMul:
                output << "fmul ";
                break;
            case BinaryOperator::Operation::FDiv:
                output << "fdiv ";
                break;
            case BinaryOperator::Operation::FRem:
                output << "frem ";
                break;
            default:
                output << "binop unknown ";
                break;
        }
        output << TypeToString(ir_binary_op->type) << " "
               << GetVariableName(ir_binary_op->GetOperand(0)) << ", "
               << GetVariableName(ir_binary_op->GetOperand(1)) << ";\n";
    }

    void Visit(std::shared_ptr<CastInstruction> ir_cast) override {
        output << GetVariableName(ir_cast) << " = ";
        switch (ir_cast->operation) {
            case CastInstruction::Operation::Trunc:
                output << "trunc ";
                break;
            case CastInstruction::Operation::ZExt:
                output << "zext ";
                break;
            case CastInstruction::Operation::SExt:
                output << "sext ";
                break;
            case CastInstruction::Operation::FPToUI:
                output << "fptoui ";
                break;
            case CastInstruction::Operation::FPToSI:
                output << "fptosi ";
                break;
            case CastInstruction::Operation::UIToFP:
                output << "uitofp ";
                break;
            case CastInstruction::Operation::SIToFP:
                output << "sitofp ";
                break;
            case CastInstruction::Operation::FPTrunc:
                output << "fptrunc ";
                break;
            case CastInstruction::Operation::FPExt:
                output << "fpext ";
                break;
            case CastInstruction::Operation::PtrToInt:
                output << "ptrtoint ";
                break;
            case CastInstruction::Operation::IntToPtr:
                output << "inttoptr ";
                break;
            case CastInstruction::Operation::BitCast:
                output << "bitcast ";
                break;
            case CastInstruction::Operation::AddrSpaceCast:
                output << "addrspacecast ";
                break;
            default:
                output << "cast unknown ";
                break;
        }
        output << TypeToString(ir_cast->GetOperand(0)->type) << " "
               << GetVariableName(ir_cast->GetOperand(0)) << " to " << TypeToString(ir_cast->type)
               << ";\n";
    }

    void Visit(std::shared_ptr<Function> ir_function) override {
        output << "define " << TypeToString(ir_function->function_type->return_type) << " @"
               << ir_function->name << "(";
        bool first = true;
        for (auto ir_param : ir_function->parameters) {
            if (!first) output << ", ";
            output << TypeToString(ir_param->type) << " " << GetVariableName(ir_param);
            first = false;
        }
        output << ") ";
        if (ir_function->IsDeclaration()) {
            output << ";\n";
        } else {
            for (auto ir_block : ir_function->blocks) {
                ir_block->ApplyVisitor(this->shared_from_this());
            }
        }
    }

    void Visit(std::shared_ptr<Block> ir_block) override {
        output << "{\n";
        ++indent_level;
        for (auto ir_value : *ir_block) {
            this->Indent();
            ir_value->ApplyVisitor(this->shared_from_this());
        }
        --indent_level;
        this->Indent();
        output << "}\n";
    }

    void Visit(std::shared_ptr<If> ir_if) override {
        output << "If " << GetVariableName(ir_if->Condition()) << " Then ";
        for (auto ir_value : *ir_if->TrueBlock()) {
            ir_value->ApplyVisitor(this->shared_from_this());
        }
        if (ir_if->FalseBlock()) {
            this->Indent();
            output << "else ";
            for (auto ir_value : *ir_if->FalseBlock()) {
                ir_value->ApplyVisitor(this->shared_from_this());
            }
        }
    }

    void Visit(std::shared_ptr<While> ir_while) override {
        output << "While " << GetVariableName(ir_while->Condition()) << " "
               << "{\n";
        ++indent_level;
        for (auto ir_value : *ir_while->ConditionBlock()) {
            this->Indent();
            ir_value->ApplyVisitor(this->shared_from_this());
        }
        --indent_level;
        this->Indent();
        output << "}\n";

        this->Indent();
        output << "Do ";
        for (auto ir_value : *ir_while->LoopBlock()) {
            ir_value->ApplyVisitor(this->shared_from_this());
        }
    }

    void Visit(std::shared_ptr<For> ir_for) override {
        output << "For " << GetVariableName(ir_for->IndexVariable()) << " = "
               << GetVariableName(ir_for->First()) << " to " << GetVariableName(ir_for->Last());
        for (auto ir_value : *ir_for->LoopBlock()) {
            ir_value->ApplyVisitor(this->shared_from_this());
        }
    }

    void Visit(std::shared_ptr<Module> ir_module) override {
        output << "Module " << GetVariableName(ir_module) << " {\n";
        ++indent_level;
        for (auto ir_global_var : ir_module->global_variables) {
            this->Indent();
            ir_global_var->ApplyVisitor(this->shared_from_this());
        }
        for (auto ir_global_alloca : ir_module->global_allocas) {
            this->Indent();
            ir_global_alloca->ApplyVisitor(this->shared_from_this());
        }
        for (auto ir_function : ir_module->functions) {
            this->Indent();  // 每个函数保持同样的缩进
            ir_function->ApplyVisitor(this->shared_from_this());
        }
        --indent_level;
        this->Indent();
        output << "}\n";
    }

    void Visit(std::shared_ptr<ValueAny> ir_value_any) override {
        output << GetVariableName(ir_value_any) << " = ValueAny;\n";
    }

    void Visit(std::shared_ptr<KernelFunctionCall> ir_kernel_function_call) override {
        output << GetVariableName(ir_kernel_function_call) << " = KernelFunctionCall "
               << GetVariableName(ir_kernel_function_call->Function()) << "(";
        for (int64_t i = 0; i < ir_kernel_function_call->ArgumentSize(); ++i) {
            if (i > 0) output << ", ";
            output << GetVariableName(ir_kernel_function_call->Argument(i));
        }
        output << ") grid " << GetVariableName(ir_kernel_function_call->GridShape()) << " block "
               << GetVariableName(ir_kernel_function_call->BlockShape()) << ";\n";
    }

    void Visit(std::shared_ptr<Closure> ir_closure) override {
        output << GetVariableName(ir_closure) << " = Closure "
               << TypeToString(ir_closure->function_type) << " {\n";
        ++indent_level;
        for (auto ir_param : ir_closure->parameters) {
            this->Indent();
            ir_param->ApplyVisitor(this->shared_from_this());
        }
        for (auto ir_block : ir_closure->blocks) {
            this->Indent();
            ir_block->ApplyVisitor(this->shared_from_this());
        }
        --indent_level;
        output << "}\n";
    }

    void Visit(std::shared_ptr<InlineAsm> ir_inline_asm) override {
        output << GetVariableName(ir_inline_asm) << " = inline_asm "
               << TypeToString(ir_inline_asm->type) << " \"" << ir_inline_asm->str_asm << "\", \""
               << ir_inline_asm->str_constrains << "\"";
        if (ir_inline_asm->has_side_effects) output << " sideeffect";
        if (ir_inline_asm->is_align_stack) output << " alignstack";
        output << ";\n";
    }

    void Visit(std::shared_ptr<Instruction> ir_instruction) override {
        output << GetVariableName(ir_instruction) << " = Instruction;\n";
    }

   private:
    std::ostringstream output;
    int indent_level = 0;  // 缩进级别
    int var_counter = 0;   // 寄存器计数器
    std::unordered_map<std::shared_ptr<Value>, std::string> var_name_dict;

    void Indent() { output << std::string(indent_level * 2, ' '); }

    void Indent(int indent_level) { output << std::string(indent_level * 2, ' '); }
    std::string TypeToString(std::shared_ptr<Type> type) {
        if (!type) {
            std::cout << type;
            return "void";
        }
        return type->name;
    }
    // 获取变量名
    std::string GetVariableName(std::shared_ptr<Value> value) {
        if (var_name_dict.find(value) != var_name_dict.end()) {
            return var_name_dict[value];
        }
        std::string name;
        if (!value->name.empty() && value->name != "NameIsUndefined") {
            return "%" + value->name;
        } else {
            name = "%" + std::to_string(var_counter++);
        }
        var_name_dict[value] = name;
        return name;
    }
};

}  // namespace prajna::ir
