#pragma once
#include "value.hpp"
#include "operation_instruction.hpp"
#include "prajna/ir/target.hpp"

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
class ConstantNull;
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
class ConditionBranch;
class JumpBranch;
class Label;
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

class Visitor : public std::enable_shared_from_this<Visitor> {
   public:
    virtual void Visit(std::shared_ptr<Instruction> ir_instruction, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Block> ir_block, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Function> ir_function, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Module> ir_module, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Value> ir_value, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<VoidValue> ir_void_value, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Parameter> ir_parameter, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Constant> ir_constant, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ConstantBool> ir_constant_bool,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ConstantRealNumber> ir_constant_real_number,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ConstantInt> ir_constant_int, prajna::ir::Target ir_target) {
    }
    virtual void Visit(std::shared_ptr<ConstantFloat> ir_constant_float,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ConstantChar> ir_constant_char,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ConstantNull> ir_constant_null,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ConstantArray> ir_constant_array,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ConstantVector> ir_constant_vector,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<MemberFunctionWithThisPointer> ir_member_function,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<WriteReadAble> ir_write_read_able,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<VariableLiked> ir_variable_liked,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Variable> ir_variable, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<LocalVariable> ir_local_variable,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<AccessField> ir_access_field, prajna::ir::Target ir_target) {
    }
    virtual void Visit(std::shared_ptr<IndexArray> ir_index_array, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<IndexPointer> ir_index_pointer,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<GetStructElementPointer> ir_get_struct_element_pointer,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<GetArrayElementPointer> ir_get_array_element_pointer,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<GetPointerElementPointer> ir_get_pointer_element_pointer,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<DeferencePointer> ir_deference_pointer,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<WriteVariableLiked> ir_write_variable_liked,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<GetAddressOfVariableLiked> ir_get_address_of_variable_liked,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Alloca> ir_alloca, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<GlobalVariable> ir_global_variable,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<GlobalAlloca> ir_global_alloca,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<LoadPointer> ir_load_pointer, prajna::ir::Target ir_target) {
    }
    virtual void Visit(std::shared_ptr<StorePointer> ir_store_pointer,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Return> ir_return, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<BitCast> ir_bit_cast, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Call> ir_call, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ConditionBranch> ir_condition_branch,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<JumpBranch> ir_jump_branch, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Label> ir_label, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ValueCollection> ir_value_collection,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<If> ir_if, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<While> ir_while, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<For> ir_for, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Break> ir_break, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Continue> ir_continue, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<AccessProperty> ir_access_property,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<WriteProperty> ir_write_property,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ShuffleVector> ir_shuffle_vector,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<ValueAny> ir_value_any, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<KernelFunctionCall> ir_kernel_function_call,
                       prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<Closure> ir_closure, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<InlineAsm> ir_inline_asm, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<CastInstruction> ir_cast_instruction, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<CompareInstruction> ir_compare_instruction, prajna::ir::Target ir_target) {}
    virtual void Visit(std::shared_ptr<BinaryOperator> ir_compare_instruction, prajna::ir::Target ir_target) {}

    virtual ~Visitor() = default;
};

}  // namespace prajna::ir