#pragma once

#include "prajna/ir/ir.hpp"

namespace prajna::ir {

class CallbackVisitor : public Visitor {
   public:
    CallbackVisitor(std::function<void(std::shared_ptr<Value>)> callback) : callback(callback) {}

    virtual void Visit(std::shared_ptr<Instruction> ir_instruction) {
        this->callback(ir_instruction);
    }
    virtual void Visit(std::shared_ptr<Block> ir_block) { this->callback(ir_block); }
    virtual void Visit(std::shared_ptr<Function> ir_function) { this->callback(ir_function); }
    virtual void Visit(std::shared_ptr<Module> ir_module) { this->callback(ir_module); }
    virtual void Visit(std::shared_ptr<Value> ir_value) { this->callback(ir_value); }
    virtual void Visit(std::shared_ptr<VoidValue> ir_void_value) { this->callback(ir_void_value); }
    virtual void Visit(std::shared_ptr<Parameter> ir_parameter) { this->callback(ir_parameter); }
    virtual void Visit(std::shared_ptr<Constant> ir_constant) { this->callback(ir_constant); }
    virtual void Visit(std::shared_ptr<ConstantBool> ir_constant_bool) {
        this->callback(ir_constant_bool);
    }
    virtual void Visit(std::shared_ptr<ConstantRealNumber> ir_constant_real_number) {
        this->callback(ir_constant_real_number);
    }
    virtual void Visit(std::shared_ptr<ConstantInt> ir_constant_int) {
        this->callback(ir_constant_int);
    }
    virtual void Visit(std::shared_ptr<ConstantFloat> ir_constant_float) {
        this->callback(ir_constant_float);
    }
    virtual void Visit(std::shared_ptr<ConstantChar> ir_constant_char) {
        this->callback(ir_constant_char);
    }
    virtual void Visit(std::shared_ptr<ConstantArray> ir_constant_array) {
        this->callback(ir_constant_array);
    }
    virtual void Visit(std::shared_ptr<ConstantVector> ir_constant_vector) {
        this->callback(ir_constant_vector);
    }
    virtual void Visit(std::shared_ptr<MemberFunctionWithThisPointer> ir_member_function) {
        this->callback(ir_member_function);
    }
    virtual void Visit(std::shared_ptr<WriteReadAble> ir_write_read_able) {
        this->callback(ir_write_read_able);
    }
    virtual void Visit(std::shared_ptr<VariableLiked> ir_variable_liked) {
        this->callback(ir_variable_liked);
    }
    virtual void Visit(std::shared_ptr<Variable> ir_variable) { this->callback(ir_variable); }
    virtual void Visit(std::shared_ptr<LocalVariable> ir_local_variable) {
        this->callback(ir_local_variable);
    }
    virtual void Visit(std::shared_ptr<AccessField> ir_access_field) {
        this->callback(ir_access_field);
    }
    virtual void Visit(std::shared_ptr<IndexArray> ir_index_array) {
        this->callback(ir_index_array);
    }
    virtual void Visit(std::shared_ptr<IndexPointer> ir_index_pointer) {
        this->callback(ir_index_pointer);
    }
    virtual void Visit(std::shared_ptr<GetStructElementPointer> ir_get_struct_element_pointer) {
        this->callback(ir_get_struct_element_pointer);
    }
    virtual void Visit(std::shared_ptr<GetArrayElementPointer> ir_get_array_element_pointer) {
        this->callback(ir_get_array_element_pointer);
    }
    virtual void Visit(std::shared_ptr<GetPointerElementPointer> ir_get_pointer_element_pointer) {
        this->callback(ir_get_pointer_element_pointer);
    }
    virtual void Visit(std::shared_ptr<DeferencePointer> ir_deference_pointer) {
        this->callback(ir_deference_pointer);
    }
    virtual void Visit(std::shared_ptr<WriteVariableLiked> ir_write_variable_liked) {
        this->callback(ir_write_variable_liked);
    }
    virtual void Visit(
        std::shared_ptr<GetAddressOfVariableLiked> ir_get_address_of_variable_liked) {
        this->callback(ir_get_address_of_variable_liked);
    }
    virtual void Visit(std::shared_ptr<Alloca> ir_alloca) { this->callback(ir_alloca); }
    virtual void Visit(std::shared_ptr<GlobalVariable> ir_global_variable) {
        this->callback(ir_global_variable);
    }
    virtual void Visit(std::shared_ptr<GlobalAlloca> ir_global_alloca) {
        this->callback(ir_global_alloca);
    }
    virtual void Visit(std::shared_ptr<LoadPointer> ir_load_pointer) {
        this->callback(ir_load_pointer);
    }
    virtual void Visit(std::shared_ptr<StorePointer> ir_store_pointer) {
        this->callback(ir_store_pointer);
    }
    virtual void Visit(std::shared_ptr<Return> ir_return) { this->callback(ir_return); }
    virtual void Visit(std::shared_ptr<BitCast> ir_bit_cast) { this->callback(ir_bit_cast); }
    virtual void Visit(std::shared_ptr<Call> ir_call) { this->callback(ir_call); }
    virtual void Visit(std::shared_ptr<Select> ir_select) { this->callback(ir_select); }
    virtual void Visit(std::shared_ptr<internal::ConditionBranch> ir_condition_branch) {
        this->callback(ir_condition_branch);
    }
    virtual void Visit(std::shared_ptr<internal::JumpBranch> ir_jump_branch) {
        this->callback(ir_jump_branch);
    }
    virtual void Visit(std::shared_ptr<internal::Label> ir_label) { this->callback(ir_label); }
    virtual void Visit(std::shared_ptr<ValueCollection> ir_value_collection) {
        this->callback(ir_value_collection);
    }
    virtual void Visit(std::shared_ptr<If> ir_if) { this->callback(ir_if); }
    virtual void Visit(std::shared_ptr<While> ir_while) { this->callback(ir_while); }
    virtual void Visit(std::shared_ptr<For> ir_for) { this->callback(ir_for); }
    virtual void Visit(std::shared_ptr<Break> ir_break) { this->callback(ir_break); }
    virtual void Visit(std::shared_ptr<Continue> ir_continue) { this->callback(ir_continue); }
    virtual void Visit(std::shared_ptr<AccessProperty> ir_access_property) {
        this->callback(ir_access_property);
    }
    virtual void Visit(std::shared_ptr<WriteProperty> ir_write_property) {
        this->callback(ir_write_property);
    }
    virtual void Visit(std::shared_ptr<ShuffleVector> ir_shuffle_vector) {
        this->callback(ir_shuffle_vector);
    }
    virtual void Visit(std::shared_ptr<ValueAny> ir_value_any) { this->callback(ir_value_any); }
    virtual void Visit(std::shared_ptr<KernelFunctionCall> ir_kernel_function_call) {
        this->callback(ir_kernel_function_call);
    }
    virtual void Visit(std::shared_ptr<Closure> ir_closure) { this->callback(ir_closure); }
    virtual void Visit(std::shared_ptr<InlineAsm> ir_inline_asm) { this->callback(ir_inline_asm); }
    virtual void Visit(std::shared_ptr<CastInstruction> ir_cast_instruction) {
        this->callback(ir_cast_instruction);
    }
    virtual void Visit(std::shared_ptr<CompareInstruction> ir_compare_instruction) {
        this->callback(ir_compare_instruction);
    }
    virtual void Visit(std::shared_ptr<BinaryOperator> ir_compare_instruction) {
        this->callback(ir_compare_instruction);
    }

   protected:
    std::function<void(std::shared_ptr<ir::Value>)> callback;
};

}  // namespace prajna::ir
