#pragma once
#include "value.hpp"

namespace prajna::ir {

class Instruction;
class Block;
class Function;
class Module;
class Value;
class VoidValue;
class Parameter;
class Constant;
class ConstantBool;
class ConstantRealNumber;
class ConstantInt;
class ConstantFloat;
class ConstantChar;
class ConstantArray;
class ConstantVector;
class Block;
class MemberFunctionWithThisPointer;
class WriteReadAble;
class VariableLiked;
class Variable;
class LocalVariable;
class AccessField;
class IndexArray;
class IndexPointer;
class GetStructElementPointer;
class GetArrayElementPointer;
class GetPointerElementPointer;
class DeferencePointer;
class WriteVariableLiked;
class GetAddressOfVariableLiked;
class Alloca;
class GlobalAlloca;
class LoadPointer;
class StorePointer;
class Return;
class BitCast;
class Call;
class Select;
class ValueCollection;
class If;
class While;
class For;
class Break;
class Continue;
class GlobalVariable;
class AccessProperty;
class WriteProperty;
class ShuffleVector;
class ValueAny;
class KernelFunctionCall;
class Closure;
class InlineAsm;
class CastInstruction;
class CompareInstruction;
class BinaryOperator;

namespace internal {
class ConditionBranch;
class JumpBranch;
class Label;
}  // namespace internal

class Visitor : public std::enable_shared_from_this<Visitor> {
   public:
    virtual void Visit(std::shared_ptr<Instruction> ir_instruction) {}
    virtual void Visit(std::shared_ptr<Block> ir_block) {}
    virtual void Visit(std::shared_ptr<Function> ir_function) {}
    virtual void Visit(std::shared_ptr<Module> ir_module) {}
    virtual void Visit(std::shared_ptr<Value> ir_value) {}
    virtual void Visit(std::shared_ptr<VoidValue> ir_void_value) {}
    virtual void Visit(std::shared_ptr<Parameter> ir_parameter) {}
    virtual void Visit(std::shared_ptr<Constant> ir_constant) {}
    virtual void Visit(std::shared_ptr<ConstantBool> ir_constant_bool) {}
    virtual void Visit(std::shared_ptr<ConstantRealNumber> ir_constant_real_number) {}
    virtual void Visit(std::shared_ptr<ConstantInt> ir_constant_int) {}
    virtual void Visit(std::shared_ptr<ConstantFloat> ir_constant_float) {}
    virtual void Visit(std::shared_ptr<ConstantChar> ir_constant_char) {}
    virtual void Visit(std::shared_ptr<ConstantArray> ir_constant_array) {}
    virtual void Visit(std::shared_ptr<ConstantVector> ir_constant_vector) {}
    virtual void Visit(std::shared_ptr<MemberFunctionWithThisPointer> ir_member_function) {}
    virtual void Visit(std::shared_ptr<WriteReadAble> ir_write_read_able) {}
    virtual void Visit(std::shared_ptr<VariableLiked> ir_variable_liked) {}
    virtual void Visit(std::shared_ptr<Variable> ir_variable) {}
    virtual void Visit(std::shared_ptr<LocalVariable> ir_local_variable) {}
    virtual void Visit(std::shared_ptr<AccessField> ir_access_field) {}
    virtual void Visit(std::shared_ptr<IndexArray> ir_index_array) {}
    virtual void Visit(std::shared_ptr<IndexPointer> ir_index_pointer) {}
    virtual void Visit(std::shared_ptr<GetStructElementPointer> ir_get_struct_element_pointer) {}
    virtual void Visit(std::shared_ptr<GetArrayElementPointer> ir_get_array_element_pointer) {}
    virtual void Visit(std::shared_ptr<GetPointerElementPointer> ir_get_pointer_element_pointer) {}
    virtual void Visit(std::shared_ptr<DeferencePointer> ir_deference_pointer) {}
    virtual void Visit(std::shared_ptr<WriteVariableLiked> ir_write_variable_liked) {}
    virtual void Visit(
        std::shared_ptr<GetAddressOfVariableLiked> ir_get_address_of_variable_liked) {}
    virtual void Visit(std::shared_ptr<Alloca> ir_alloca) {}
    virtual void Visit(std::shared_ptr<GlobalVariable> ir_global_variable) {}
    virtual void Visit(std::shared_ptr<GlobalAlloca> ir_global_alloca) {}
    virtual void Visit(std::shared_ptr<LoadPointer> ir_load_pointer) {}
    virtual void Visit(std::shared_ptr<StorePointer> ir_store_pointer) {}
    virtual void Visit(std::shared_ptr<Return> ir_return) {}
    virtual void Visit(std::shared_ptr<BitCast> ir_bit_cast) {}
    virtual void Visit(std::shared_ptr<Call> ir_call) {}
    virtual void Visit(std::shared_ptr<Select> ir_select) {}
    virtual void Visit(std::shared_ptr<internal::ConditionBranch> ir_condition_branch) {}
    virtual void Visit(std::shared_ptr<internal::JumpBranch> ir_jump_branch) {}
    virtual void Visit(std::shared_ptr<internal::Label> ir_label) {}
    virtual void Visit(std::shared_ptr<ValueCollection> ir_value_collection) {}
    virtual void Visit(std::shared_ptr<If> ir_if) {}
    virtual void Visit(std::shared_ptr<While> ir_while) {}
    virtual void Visit(std::shared_ptr<For> ir_for) {}
    virtual void Visit(std::shared_ptr<Break> ir_break) {}
    virtual void Visit(std::shared_ptr<Continue> ir_continue) {}
    virtual void Visit(std::shared_ptr<AccessProperty> ir_access_property) {}
    virtual void Visit(std::shared_ptr<WriteProperty> ir_write_property) {}
    virtual void Visit(std::shared_ptr<ShuffleVector> ir_shuffle_vector) {}
    virtual void Visit(std::shared_ptr<ValueAny> ir_value_any) {}
    virtual void Visit(std::shared_ptr<KernelFunctionCall> ir_kernel_function_call) {}
    virtual void Visit(std::shared_ptr<Closure> ir_closure) {}
    virtual void Visit(std::shared_ptr<InlineAsm> ir_inline_asm) {}
    virtual void Visit(std::shared_ptr<CastInstruction> ir_cast_instruction) {}
    virtual void Visit(std::shared_ptr<CompareInstruction> ir_compare_instruction) {}
    virtual void Visit(std::shared_ptr<BinaryOperator> ir_compare_instruction) {}

    virtual ~Visitor() = default;
};

}  // namespace prajna::ir
