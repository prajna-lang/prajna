#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <unordered_map>
// #include <value.hpp>

#include "boost/range/combine.hpp"
#include "prajna/ir/operation_instruction.hpp"
#include "prajna/ir/value.hpp"

namespace prajna::ir {

class FunctionCloner : public Visitor {
   private:
    FunctionCloner() = default;

   public:
    static std::shared_ptr<ir::FunctionCloner> Create(std::shared_ptr<ir::Module> ir_module,
                                                      bool shallow) {
        std::shared_ptr<ir::FunctionCloner> self(new FunctionCloner);
        self->ir_module = ir_module;
        self->shallow_function_copy = shallow;
        return self;
    }

    void Visit(std::shared_ptr<Block> ir_block) override {
        if (value_dict.count(ir_block)) {
            return;
        }

        auto ir_new = Block::Create();
        value_dict[ir_block] = ir_new;
        for (auto ir_tensor : *ir_block) {
            ir_tensor->ApplyVisitor(this->shared_from_this());
            ir_new->PushBack(value_dict[ir_tensor]);
        }
    }

    void Visit(std::shared_ptr<Function> ir_function) override {
        if (value_dict.count(ir_function)) {
            return;
        }

        if (this->shallow_function_copy && this->first_function_has_cloned) {
            value_dict[ir_function] = ir_function;
            return;
        }

        if (!this->first_function_has_cloned) {
            this->first_function_has_cloned = true;
        }

        std::shared_ptr<ir::Function> ir_new(new Function(*ir_function));
        //
        value_dict[ir_function] = ir_new;
        this->ir_module->AddFunction(ir_new);
        ir_new->parameters.clear();
        std::ranges::transform(ir_function->parameters, std::back_inserter(ir_new->parameters),
                               [=](auto ir_parameter) {
                                   ir_parameter->ApplyVisitor(this->shared_from_this());
                                   value_dict[ir_parameter]->parent = ir_new;
                                   return Cast<Parameter>(value_dict[ir_parameter]);
                               });

        ir_new->blocks.clear();
        for (auto ir_block : ir_function->blocks) {
            ir_block->ApplyVisitor(this->shared_from_this());
            ir_new->blocks.push_back(Cast<Block>(value_dict[ir_block]));
            value_dict[ir_block]->parent = ir_new;
        }
    }

    void Visit(std::shared_ptr<VoidValue> ir_void_value) override {
        if (value_dict.count(ir_void_value)) {
            return;
        }

        auto ir_new = VoidValue::Create();
        value_dict[ir_void_value] = ir_new;
    }

    void Visit(std::shared_ptr<Parameter> ir_parameter) override {
        if (value_dict[ir_parameter]) {
            return;
        }

        auto ir_new = Parameter::Create(ir_parameter->type);
        value_dict[ir_parameter] = ir_new;
    }

    void Visit(std::shared_ptr<ConstantBool> ir_constant_bool) override {
        if (value_dict[ir_constant_bool]) {
            return;
        }
        std::shared_ptr<ConstantBool> ir_new(new ConstantBool(*ir_constant_bool));
        value_dict[ir_constant_bool] = ir_new;
    }

    void Visit(std::shared_ptr<ConstantInt> ir_constant_int) override {
        if (value_dict[ir_constant_int]) {
            return;
        }
        std::shared_ptr<ConstantInt> ir_new(new ConstantInt(*ir_constant_int));
        value_dict[ir_constant_int] = ir_new;
    }

    void Visit(std::shared_ptr<ConstantFloat> ir_constant_float) override {
        if (value_dict[ir_constant_float]) {
            return;
        }
        std::shared_ptr<ConstantFloat> ir_new(new ConstantFloat(*ir_constant_float));
        value_dict[ir_constant_float] = ir_new;
    }

    void Visit(std::shared_ptr<ConstantChar> ir_constant_char) override {
        if (value_dict[ir_constant_char]) {
            return;
        }
        std::shared_ptr<ConstantChar> ir_new(new ConstantChar(*ir_constant_char));
        value_dict[ir_constant_char] = ir_new;
    }

    void Visit(std::shared_ptr<ConstantArray> ir_constant_array) override {
        if (value_dict[ir_constant_array]) {
            return;
        }

        std::list<std::shared_ptr<Constant>> new_initialize_constants(
            ir_constant_array->initialize_constants.size());
        std::ranges::transform(
            ir_constant_array->initialize_constants, new_initialize_constants.begin(),
            [=](auto ir_constant) {
                PRAJNA_ASSERT(this->value_dict[ir_constant]);  // constant应该在前面就处理过;
                return Cast<Constant>(this->value_dict[ir_constant]);
            });

        auto ir_array_type = Cast<ArrayType>(ir_constant_array->type);
        auto ir_new = ConstantArray::Create(ir_array_type, new_initialize_constants);
        value_dict[ir_constant_array] = ir_new;
    }

    void Visit(std::shared_ptr<ConstantVector> ir_constant_vector) override {
        if (value_dict[ir_constant_vector]) {
            return;
        }

        std::list<std::shared_ptr<Constant>> new_initialize_constants(
            ir_constant_vector->initialize_constants.size());
        std::ranges::transform(
            ir_constant_vector->initialize_constants, new_initialize_constants.begin(),
            [=](auto ir_constant) {
                PRAJNA_ASSERT(this->value_dict[ir_constant]);  // constant应该在前面就处理过;
                return Cast<Constant>(this->value_dict[ir_constant]);
            });

        std::shared_ptr<ConstantVector> ir_new = ConstantVector::Create(
            Cast<VectorType>(ir_constant_vector->type), new_initialize_constants);
        this->value_dict[ir_constant_vector] = ir_new;
    }

    void Visit(std::shared_ptr<LocalVariable> ir_local_variable) override {
        if (value_dict[ir_local_variable]) {
            return;
        }
        std::shared_ptr<LocalVariable> ir_new = LocalVariable::Create(ir_local_variable->type);
        this->value_dict[ir_local_variable] = ir_new;
    }

    void Visit(std::shared_ptr<AccessField> ir_access_field) override {
        if (value_dict[ir_access_field]) {
            return;
        }
        this->VisitOperands(ir_access_field);
        auto ir_new =
            AccessField::Create(value_dict[ir_access_field->object()], ir_access_field->field);
        this->value_dict[ir_access_field] = ir_new;
    }

    void Visit(std::shared_ptr<IndexArray> ir_index_array) override {
        if (value_dict[ir_index_array]) {
            return;
        }
        this->VisitOperands(ir_index_array);
        auto ir_new = IndexArray::Create(value_dict[ir_index_array->object()],
                                         value_dict[ir_index_array->IndexVariable()]);
        this->value_dict[ir_index_array] = ir_new;
    }

    void Visit(std::shared_ptr<IndexPointer> ir_index_pointer) override {
        if (value_dict[ir_index_pointer]) {
            return;
        }
        this->VisitOperands(ir_index_pointer);
        auto ir_new = IndexPointer::Create(value_dict[ir_index_pointer->object()],
                                           value_dict[ir_index_pointer->IndexVariable()]);
        this->value_dict[ir_index_pointer] = ir_new;
    }

    void Visit(std::shared_ptr<GetStructElementPointer> ir_get_struct_element_pointer) override {
        if (value_dict[ir_get_struct_element_pointer]) {
            return;
        }
        this->VisitOperands(ir_get_struct_element_pointer);
        auto ir_new =
            GetStructElementPointer::Create(value_dict[ir_get_struct_element_pointer->Pointer()],
                                            ir_get_struct_element_pointer->field);
        this->value_dict[ir_get_struct_element_pointer] = ir_new;
    }

    void Visit(std::shared_ptr<GetArrayElementPointer> ir_get_array_element_pointer) override {
        if (value_dict[ir_get_array_element_pointer]) {
            return;
        }
        this->VisitOperands(ir_get_array_element_pointer);
        auto ir_new = GetArrayElementPointer::Create(
            value_dict[ir_get_array_element_pointer->Pointer()],
            value_dict[ir_get_array_element_pointer->IndexVariable()]);
        this->value_dict[ir_get_array_element_pointer] = ir_new;
    }

    void Visit(std::shared_ptr<GetPointerElementPointer> ir_get_pointer_element_pointer) override {
        if (value_dict[ir_get_pointer_element_pointer]) {
            return;
        }

        this->VisitOperands(ir_get_pointer_element_pointer);
        auto ir_new = GetPointerElementPointer::Create(
            value_dict[ir_get_pointer_element_pointer->Pointer()],
            value_dict[ir_get_pointer_element_pointer->IndexVariable()]);
        this->value_dict[ir_get_pointer_element_pointer] = ir_new;
    }

    void Visit(std::shared_ptr<DeferencePointer> ir_deference_pointer) override {
        if (value_dict[ir_deference_pointer]) {
            return;
        }

        this->VisitOperands(ir_deference_pointer);
        auto ir_new = DeferencePointer::Create(value_dict[ir_deference_pointer->Pointer()]);
        this->value_dict[ir_deference_pointer] = ir_new;
    }

    void Visit(std::shared_ptr<WriteVariableLiked> ir_write_variable_liked) override {
        if (value_dict[ir_write_variable_liked]) {
            return;
        }

        this->VisitOperands(ir_write_variable_liked);
        auto ir_new = WriteVariableLiked::Create(
            value_dict[ir_write_variable_liked->Value()],
            Cast<ir::VariableLiked>(value_dict[ir_write_variable_liked->variable()]));
        this->value_dict[ir_write_variable_liked] = ir_new;
    }

    void Visit(
        std::shared_ptr<GetAddressOfVariableLiked> ir_get_address_of_variable_liked) override {
        if (value_dict[ir_get_address_of_variable_liked]) {
            return;
        }
        this->VisitOperands(ir_get_address_of_variable_liked);
        auto ir_new = GetAddressOfVariableLiked::Create(
            Cast<VariableLiked>(value_dict[ir_get_address_of_variable_liked->variable()]));
        this->value_dict[ir_get_address_of_variable_liked] = ir_new;
    }

    void Visit(std::shared_ptr<Alloca> ir_alloca) override {
        if (value_dict[ir_alloca]) {
            return;
        }
        this->VisitOperands(ir_alloca);
        auto ir_new = Alloca::Create(Cast<ir::PointerType>(ir_alloca->type)->value_type,
                                     value_dict[ir_alloca->Length()], ir_alloca->alignment);
        this->value_dict[ir_alloca] = ir_new;
    }

    void Visit(std::shared_ptr<GlobalAlloca> ir_global_alloca) override {
        // 如果已经处理过，直接返回
        if (value_dict.count(ir_global_alloca)) return;

        // 当shared memory会提前拷贝到module里
        if (std::ranges::count(this->ir_module->global_allocas, ir_global_alloca)) {
            value_dict[ir_global_alloca] = ir_global_alloca;
            return;
        } else {
            auto ir_new = GlobalAlloca::Create(ir_global_alloca->type);
            ir_new->name = ir_global_alloca->name;
            ir_new->fullname = ir_global_alloca->fullname;
            ir_new->is_external = true;
            ir_new->address_space = ir_global_alloca->address_space;
            this->ir_module->AddGlobalAlloca(ir_new);
            this->value_dict[ir_global_alloca] = ir_new;
        }
    }

    void Visit(std::shared_ptr<LoadPointer> ir_load_pointer) override {
        if (value_dict[ir_load_pointer]) {
            return;
        }
        this->VisitOperands(ir_load_pointer);
        auto ir_new = LoadPointer::Create(value_dict[ir_load_pointer->Pointer()]);
        this->value_dict[ir_load_pointer] = ir_new;
    }

    void Visit(std::shared_ptr<StorePointer> ir_store_pointer) override {
        if (value_dict[ir_store_pointer]) {
            return;
        }
        this->VisitOperands(ir_store_pointer);
        ir_store_pointer->Pointer()->ApplyVisitor(this->shared_from_this());
        auto ir_new = StorePointer::Create(value_dict[ir_store_pointer->Value()],
                                           value_dict[ir_store_pointer->Pointer()]);
        this->value_dict[ir_store_pointer] = ir_new;
    }

    void Visit(std::shared_ptr<Return> ir_return) override {
        if (value_dict[ir_return]) {
            return;
        }
        this->VisitOperands(ir_return);
        auto ir_new = Return::Create(value_dict[ir_return->Value()]);
        this->value_dict[ir_return] = ir_new;
    }

    void Visit(std::shared_ptr<BitCast> ir_bit_cast) override {
        if (value_dict[ir_bit_cast]) {
            return;
        }
        this->VisitOperands(ir_bit_cast);
        auto ir_new = BitCast::Create(value_dict[ir_bit_cast->Value()], ir_bit_cast->type);
        this->value_dict[ir_bit_cast] = ir_new;
    }

    void Visit(std::shared_ptr<Call> ir_call) override {
        if (value_dict[ir_call]) {
            return;
        }

        this->VisitOperands(ir_call);
        std::list<std::shared_ptr<ir::Value>> arguments;
        for (int64_t i = 0; i < ir_call->ArgumentSize(); ++i) {
            arguments.push_back(value_dict[ir_call->Argument(i)]);
        }
        auto ir_new = Call::Create(value_dict[ir_call->Function()], arguments);
        this->value_dict[ir_call] = ir_new;
    }

    void Visit(std::shared_ptr<Select> ir_select) override {
        if (value_dict[ir_select]) {
            return;
        }

        this->VisitOperands(ir_select);
        auto ir_new =
            Select::Create(value_dict[ir_select->Condition()], value_dict[ir_select->TrueValue()],
                           value_dict[ir_select->FalseValue()]);
        this->value_dict[ir_select] = ir_new;
    }

    void Visit(std::shared_ptr<ConditionBranch> ir_condition_branch) override {
        if (value_dict[ir_condition_branch]) {
            return;
        }
        this->VisitOperands(ir_condition_branch);
        auto ir_new =
            ConditionBranch::Create(value_dict[ir_condition_branch->Condition()],
                                    Cast<Block>(value_dict[ir_condition_branch->TrueBlock()]),
                                    Cast<Block>(value_dict[ir_condition_branch->FalseBlock()]));
        this->value_dict[ir_condition_branch] = ir_new;
    }

    void Visit(std::shared_ptr<JumpBranch> ir_jump_branch) override {
        if (value_dict[ir_jump_branch]) {
            return;
        }
        auto ir_new = JumpBranch::Create();
        this->value_dict[ir_jump_branch] = ir_new;  // 必须前置dict, 否则会产生死递归
        this->VisitOperands(ir_jump_branch);
        ir_new->NextBlock(Cast<Block>(value_dict[ir_jump_branch->NextBlock()]));
    }

    void Visit(std::shared_ptr<Label> ir_label) override {
        if (value_dict[ir_label]) {
            return;
        }
        std::shared_ptr<Label> ir_new = Label::Create();
        this->value_dict[ir_label] = ir_new;
    }

    void Visit(std::shared_ptr<If> ir_if) override {
        if (value_dict[ir_if]) {
            return;
        }
        this->VisitOperands(ir_if);
        auto ir_new =
            If::Create(value_dict[ir_if->Condition()], Cast<Block>(value_dict[ir_if->TrueBlock()]),
                       Cast<Block>(value_dict[ir_if->FalseBlock()]));
        this->value_dict[ir_if] = ir_new;
    }

    void VisitOperands(std::shared_ptr<ir::Instruction> ir_instruction) {
        for (int64_t i = 0; i < ir_instruction->OperandSize(); ++i) {
            auto ir_operand = ir_instruction->GetOperand(i);
            if (value_dict.count(ir_operand)) {
                continue;
            }
            ir_operand->ApplyVisitor(this->shared_from_this());
        }
    }

    void Visit(std::shared_ptr<While> ir_while) override {
        if (value_dict[ir_while]) {
            return;
        }

        this->VisitOperands(ir_while);
        auto ir_new = While::Create(value_dict[ir_while->Condition()],
                                    Cast<Block>(value_dict[ir_while->ConditionBlock()]),
                                    Cast<Block>(value_dict[ir_while->LoopBlock()]));
        this->value_dict[ir_while] = ir_new;
    }

    void Visit(std::shared_ptr<For> ir_for) override {
        if (value_dict[ir_for]) {
            return;
        }

        this->VisitOperands(ir_for);
        auto ir_new = For::Create(Cast<LocalVariable>(value_dict[ir_for->IndexVariable()]),
                                  value_dict[ir_for->First()], value_dict[ir_for->Last()],
                                  Cast<Block>(value_dict[ir_for->LoopBlock()]));
        this->value_dict[ir_for] = ir_new;
    }

    void Visit(std::shared_ptr<Break> ir_break) override {
        if (value_dict[ir_break]) {
            return;
        }

        this->VisitOperands(ir_break);
        auto ir_new = Break::Create(value_dict[ir_break->Loop()]);
        this->value_dict[ir_break] = ir_new;
    }

    void Visit(std::shared_ptr<Continue> ir_continue) override {
        if (value_dict[ir_continue]) {
            return;
        }

        this->VisitOperands(ir_continue);
        auto ir_new = Continue::Create(value_dict[ir_continue->Loop()]);
        this->value_dict[ir_continue] = ir_new;
    }

    void Visit(std::shared_ptr<AccessProperty> ir_access_property) override {
        if (value_dict[ir_access_property]) {
            return;
        }
        this->VisitOperands(ir_access_property);
        auto ir_new = AccessProperty::Create(value_dict[ir_access_property->ThisPointer()],
                                             ir_access_property->property);
        this->value_dict[ir_access_property] = ir_new;
    }

    void Visit(std::shared_ptr<WriteProperty> ir_write_property) override {
        if (value_dict[ir_write_property]) {
            return;
        }

        this->VisitOperands(ir_write_property);
        auto ir_new = WriteProperty::Create(value_dict[ir_write_property->Value()],
                                            ir_write_property->property());
        this->value_dict[ir_write_property] = ir_new;
    }

    void Visit(std::shared_ptr<ShuffleVector> ir_shuffle_vector) override {
        if (value_dict[ir_shuffle_vector]) {
            return;
        }

        this->VisitOperands(ir_shuffle_vector);
        auto ir_new = ShuffleVector::Create(value_dict[ir_shuffle_vector->Value()],
                                            value_dict[ir_shuffle_vector->Mask()]);
        this->value_dict[ir_shuffle_vector] = ir_new;
    }

    void Visit(std::shared_ptr<CompareInstruction> ir_compare_instruction) override {
        if (value_dict[ir_compare_instruction]) {
            return;
        }

        this->VisitOperands(ir_compare_instruction);
        auto ir_new = CompareInstruction::Create(ir_compare_instruction->operation,
                                                 value_dict[ir_compare_instruction->GetOperand(0)],
                                                 value_dict[ir_compare_instruction->GetOperand(1)]);
        this->value_dict[ir_compare_instruction] = ir_new;
    }

    void Visit(std::shared_ptr<BinaryOperator> ir_binary_operator) override {
        if (value_dict[ir_binary_operator]) {
            return;
        }

        this->VisitOperands(ir_binary_operator);
        auto ir_new = BinaryOperator::Create(ir_binary_operator->operation,
                                             value_dict[ir_binary_operator->GetOperand(0)],
                                             value_dict[ir_binary_operator->GetOperand(1)]);
        this->value_dict[ir_binary_operator] = ir_new;
    }

    void Visit(std::shared_ptr<CastInstruction> ir_cast_instruction) override {
        if (value_dict[ir_cast_instruction]) {
            return;
        }

        this->VisitOperands(ir_cast_instruction);
        auto ir_new = CastInstruction::Create(ir_cast_instruction->operation,
                                              value_dict[ir_cast_instruction->GetOperand(0)],
                                              ir_cast_instruction->type);
        this->value_dict[ir_cast_instruction] = ir_new;
    }

    std::shared_ptr<Value> Clone(std::shared_ptr<Value> ir_value) {
        ir_value->ApplyVisitor(this->shared_from_this());
        return value_dict[ir_value];
    }

    std::unordered_map<std::shared_ptr<ir::Value>, std::shared_ptr<ir::Value>> value_dict;

    bool shallow_function_copy = false;
    bool first_function_has_cloned = false;
    std::shared_ptr<ir::Module> ir_module = nullptr;
};

}  // namespace prajna::ir
