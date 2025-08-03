#pragma once

#include <any>
#include <functional>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "boost/noncopyable.hpp"
#include "boost/range/combine.hpp"
#include "prajna/ast/ast.hpp"
#include "prajna/helper.hpp"
#include "prajna/ir/global_context.h"
#include "prajna/ir/target.hpp"
#include "prajna/ir/type.hpp"
#include "prajna/ir/visitor.hpp"

namespace prajna::ir {

class Instruction;

struct InstructionAndOperandIndex {
    std::weak_ptr<Instruction> instruction;
    int64_t operand_index;
};

inline bool operator==(prajna::ir::InstructionAndOperandIndex lhs,
                       prajna::ir::InstructionAndOperandIndex rhs) {
    return prajna::Lock(lhs.instruction) == prajna::Lock(rhs.instruction) &&
           lhs.operand_index == rhs.operand_index;
}

}  // namespace prajna::ir

template <>
struct std::hash<prajna::ir::InstructionAndOperandIndex> {
    std::int64_t operator()(prajna::ir::InstructionAndOperandIndex inst_with_idx) const noexcept {
        std::int64_t h1 = std::hash<std::shared_ptr<prajna::ir::Instruction>>{}(
            prajna::Lock(inst_with_idx.instruction));
        std::int64_t h2 = std::hash<int64_t>{}(inst_with_idx.operand_index);
        // 这里哈希函数应该不重要, 应该不会导致性能问题
        return h1 ^ (h2 << 1);
    }
};

namespace llvm {

class Value;
class Module;

}  // namespace llvm

namespace prajna::ir {

class Block;
class Function;
class Module;

class Value : public Named, public std::enable_shared_from_this<Value> {
   protected:
    Value() {}

    Value(const Value& other) {
        this->type = other.type;
        this->annotation_dict = other.annotation_dict;
        this->tag = other.tag;

        this->name = other.name;
        this->fullname = other.fullname;

        this->parent.reset();
        this->instruction_with_index_list.clear();
        this->llvm_value = nullptr;
    }

   public:
    static std::shared_ptr<ir::Value> Create(std::shared_ptr<Type> ir_type) {
        PRAJNA_ASSERT(ir_type);
        std::shared_ptr<ir::Value> self(new Value);
        self->type = ir_type;
        self->tag = "Value";
        return self;
    }

    virtual ~Value() {}

    /// @brief 释放不必要的依赖, 解除循环引用
    virtual void Detach() {
        // 只是解除依赖, 不是销毁数据,
        this->instruction_with_index_list.clear();
        this->parent.reset();
    }

    /// @brief 实例需要销毁前调用
    virtual void Finalize();

    std::shared_ptr<Function> GetParentFunction();

    std::shared_ptr<FunctionType> GetFunctionType() {
        if (auto ir_pointer_type = Cast<PointerType>(this->type)) {
            if (auto ir_function_type = Cast<FunctionType>(ir_pointer_type->value_type)) {
                return ir_function_type;
            }
        }

        return nullptr;
    }

    std::list<std::shared_ptr<ir::Value>>::iterator GetBlockIterator();

    std::shared_ptr<Block> GetRootBlock();

    std::shared_ptr<Block> GetParentBlock() {
        auto ir_parent_block = Cast<ir::Block>(Lock(this->parent));
        PRAJNA_ASSERT(ir_parent_block);
        return ir_parent_block;
    }

    std::shared_ptr<Module> GetParentModule() { return Cast<ir::Module>(this->parent.lock()); }

    bool IsFunction() { return GetFunctionType() != nullptr; }

    virtual void ApplyVisitor(std::shared_ptr<Visitor> interpreter) { PRAJNA_UNREACHABLE; }

   private:
    bool is_finalized = false;

   public:
    std::shared_ptr<Type> type = nullptr;
    std::unordered_map<std::string, std::list<std::string>> annotation_dict;
    std::weak_ptr<Value> parent;

    std::list<InstructionAndOperandIndex> instruction_with_index_list;
    ast::SourceLocation source_location;
    llvm::Value* llvm_value = nullptr;
    // 用于方便调试, 否则无法有效辨别他们
    std::string tag = "";
};

class VoidValue : public Value {
   protected:
    VoidValue() = default;

   public:
    static std::shared_ptr<VoidValue> Create() {
        std::shared_ptr<VoidValue> self(new VoidValue);
        self->type = VoidType::Create();
        self->tag = "VoidValue";
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<VoidValue>(this->shared_from_this()));
    }
};

class Parameter : public Value {
   protected:
    Parameter() = default;

   public:
    static std::shared_ptr<Parameter> Create(std::shared_ptr<Type> ir_type) {
        PRAJNA_ASSERT(ir_type);
        std::shared_ptr<Parameter> self(new Parameter);
        self->type = ir_type;
        self->tag = "Parameter";
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Parameter>(this->shared_from_this()));
    }

   public:
    bool no_alias = false;
    bool no_capture = false;
    bool no_undef = false;
    bool readonly = false;
};

class Constant : public Value {
   protected:
    Constant() = default;

   public:
};

class ConstantBool : public Constant {
   protected:
    ConstantBool() = default;

   public:
    static std::shared_ptr<ConstantBool> Create(bool value) {
        std::shared_ptr<ConstantBool> self(new ConstantBool);
        self->type = BoolType::Create();
        self->value = value;
        self->tag = "ConstantBool";
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<ConstantBool>(this->shared_from_this()));
    }

   public:
    bool value;
};

class ConstantRealNumber : public Constant {
   protected:
    ConstantRealNumber() = default;
};

class ConstantInt : public ConstantRealNumber {
   protected:
    ConstantInt() = default;

   public:
    static std::shared_ptr<ConstantInt> Create(std::shared_ptr<Type> ir_type, int64_t value) {
        PRAJNA_ASSERT(ir_type);
        std::shared_ptr<ConstantInt> self(new ConstantInt);
        self->type = ir_type;
        self->value = value;
        self->tag = "ConstantInt";
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<ConstantInt>(this->shared_from_this()));
    }

   public:
    uint64_t value;
};

class ConstantFloat : public Constant {
   protected:
    ConstantFloat() = default;

   public:
    enum SpecialValue { None, Smallest, Largest, NaN, Inf };

    static std::shared_ptr<ConstantFloat> Create(std::shared_ptr<Type> type, double value) {
        PRAJNA_ASSERT(type);
        std::shared_ptr<ConstantFloat> self(new ConstantFloat);
        self->type = type;
        self->value = value;
        self->tag = "ConstantFloat";
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<ConstantFloat>(this->shared_from_this()));
    }

   public:
    double value;
    SpecialValue special_value = SpecialValue::None;
    bool is_negative = false;
};

class ConstantChar : public Constant {
   protected:
    ConstantChar() = default;

   public:
    static std::shared_ptr<ConstantChar> Create(char value) {
        std::shared_ptr<ConstantChar> self(new ConstantChar);
        self->type = CharType::Create();
        self->value = value;
        self->tag = "ConstantChar";
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<ConstantChar>(this->shared_from_this()));
    }

   public:
    char value;
};

class ConstantArray : public Constant {
   protected:
    ConstantArray() = default;

   public:
    static std::shared_ptr<ConstantArray> Create(
        std::shared_ptr<ArrayType> ir_array_type,
        std::list<std::shared_ptr<Constant>> ir_init_constants) {
        PRAJNA_ASSERT(ir_array_type);
        std::shared_ptr<ConstantArray> self(new ConstantArray);
        self->tag = "ConstantArray";
        self->type = ir_array_type;
        self->initialize_constants = ir_init_constants;
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<ConstantArray>(this->shared_from_this()));
    }

    std::list<std::shared_ptr<Constant>> initialize_constants;
};

class ConstantVector : public Constant {
   protected:
    ConstantVector() = default;

   public:
    static std::shared_ptr<ConstantVector> Create(
        std::shared_ptr<VectorType> ir_vector_type,
        std::list<std::shared_ptr<Constant>> ir_init_constants) {
        PRAJNA_ASSERT(ir_vector_type);
        std::shared_ptr<ConstantVector> self(new ConstantVector);
        self->tag = "ConstantVector";
        self->type = ir_vector_type;
        self->initialize_constants = ir_init_constants;
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<ConstantVector>(this->shared_from_this()));
    }

    std::list<std::shared_ptr<Constant>> initialize_constants;
};

/// @brief  和高级语言里的块是对应的
class Block : public Value, public std::list<std::shared_ptr<ir::Value>> {
   protected:
    Block() = default;

   public:
    using iterator = std::list<std::shared_ptr<ir::Value>>::iterator;

    static std::shared_ptr<Block> Create() {
        std::shared_ptr<Block> self(new Block);
        self->type = nullptr;
        self->tag = "Block";
        return self;
    }

    iterator Insert(iterator iter, std::shared_ptr<ir::Value> ir_value) {
        ir_value->parent = Cast<Block>(this->shared_from_this());
        return this->insert(iter, ir_value);
    }

    iterator Find(std::shared_ptr<ir::Value> ir_value) {
        PRAJNA_ASSERT(ir_value->GetParentBlock().get() == this);
        return std::ranges::find(*this, ir_value);
    }

    iterator Erase(iterator iter) {
        PRAJNA_ASSERT((*iter) && (*iter)->GetParentBlock() == shared_from_this());
        (*iter)->parent.reset();
        return this->erase(iter);
    }

    void Remove(std::shared_ptr<ir::Value> ir_value) {
        ir_value->parent.reset();
        this->Erase(std::ranges::find(*this, ir_value));
    }

    void PushFront(std::shared_ptr<ir::Value> ir_value) {
        ir_value->parent = Cast<Block>(this->shared_from_this());
        this->push_front(ir_value);
    }

    void PushBack(std::shared_ptr<ir::Value> ir_value) {
        ir_value->parent = Cast<Block>(this->shared_from_this());
        this->push_back(ir_value);
    }

    void Detach() override {
        Value::Detach();
        this->parent.reset();
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Block>(this->shared_from_this()));
    }
};

class Function : public Value {
   protected:
    Function() = default;

   public:
    static std::shared_ptr<Function> Create(std::shared_ptr<FunctionType> function_type) {
        PRAJNA_ASSERT(function_type);
        std::shared_ptr<Function> self(new Function);
        self->function_type = function_type;
        self->Update();
        self->tag = "Function";
        return self;
    }

    void Update() {
        // @warning 事实上llvm::Function是一个指针类型
        this->parameters.resize(function_type->parameter_types.size());
        this->type = PointerType::Create(function_type);
        std::ranges::transform(function_type->parameter_types, this->parameters.begin(),
                               [=](std::shared_ptr<Type> ir_argument_type) {
                                   auto ir_parameter = Parameter::Create(ir_argument_type);
                                   ir_parameter->parent =
                                       Cast<ir::Function>(this->shared_from_this());
                                   return ir_parameter;
                               });
    }

    void Detach() override {
        Value::Detach();
        for (auto& ir_parameter : this->parameters) {
            ir_parameter->Detach();
        }
        this->parent.reset();
    }

    void Finalize() override {
        Value::Finalize();
        this->parent.reset();
        this->function_type = nullptr;
        this->parameters.clear();
        this->blocks.clear();
    }

    bool IsDeclaration() { return this->blocks.empty(); }
    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        auto self = this->shared_from_this();
        interpreter->Visit(Cast<Function>(self));
    }

   public:
    std::shared_ptr<FunctionType> function_type = nullptr;
    std::list<std::shared_ptr<ir::Parameter>> parameters;
    std::list<std::shared_ptr<Block>> blocks;
    std::shared_ptr<ir::Value> closure = nullptr;
};

/// @brief 在lowering时需要用到的辅助IR, 并不应该在lowering后出现
class MemberFunctionWithThisPointer : public Value {
   protected:
    MemberFunctionWithThisPointer() = default;

   public:
    static std::shared_ptr<MemberFunctionWithThisPointer> Create(
        std::shared_ptr<ir::Value> ir_this_pointer, std::shared_ptr<Function> ir_global_function) {
        PRAJNA_ASSERT(ir_this_pointer);
        PRAJNA_ASSERT(ir_global_function);
        std::shared_ptr<MemberFunctionWithThisPointer> self(new MemberFunctionWithThisPointer);
        self->this_pointer = ir_this_pointer;
        self->function_prototype = ir_global_function;
        self->tag = "MemberFunctionWithThisPointer";
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<MemberFunctionWithThisPointer>(this->shared_from_this()));
    }

    std::shared_ptr<ir::Value> this_pointer = nullptr;
    std::shared_ptr<Function> function_prototype = nullptr;
};

class WriteReadAble : virtual public Value {
   protected:
    WriteReadAble() = default;

    bool is_readable = true;
    bool is_writeable = true;

   public:
    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<WriteReadAble>(this->shared_from_this()));
    }
};

class VariableLiked : virtual public WriteReadAble {
   protected:
    VariableLiked() = default;

   public:
    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<VariableLiked>(this->shared_from_this()));
    }
};

class Variable : public VariableLiked {
   protected:
    Variable() = default;

   public:
    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Variable>(this->shared_from_this()));
    }
};

class LocalVariable : public Variable {
   protected:
    LocalVariable() = default;

   public:
    static std::shared_ptr<LocalVariable> Create(std::shared_ptr<Type> type) {
        PRAJNA_ASSERT(!Is<VoidType>(type));
        std::shared_ptr<LocalVariable> self(new LocalVariable);
        self->type = type;
        self->tag = "LocalVariable";
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<LocalVariable>(this->shared_from_this()));
    }
};

/// @brief

class Instruction : virtual public Value {
   protected:
    Instruction() : Instruction(0) {}

    Instruction(int64_t operand_size) { this->operands.resize(operand_size); }

   public:
    virtual void OperandResize(int64_t size) { return this->operands.resize(size); }

    virtual int64_t OperandSize() { return this->operands.size(); }

    virtual std::shared_ptr<ir::Value> GetOperand(int64_t i) {
        PRAJNA_ASSERT(this->OperandSize() > i);
        return this->operands[i];
    };

    virtual void SetOperand(int64_t i, std::shared_ptr<ir::Value> ir_value) {
        PRAJNA_ASSERT(ir_value);
        PRAJNA_ASSERT(this->OperandSize() > i);

        auto ir_old_value = this->operands[i];
        if (ir_old_value) {
            ir_old_value->instruction_with_index_list.remove(
                {Cast<Instruction>(this->shared_from_this()), i});
        }

        this->operands[i] = ir_value;
        if (ir_value)
            ir_value->instruction_with_index_list.push_back(
                {Cast<Instruction>(this->shared_from_this()), i});
    };

    void Finalize() override {
        Value::Finalize();

        for (int64_t i = 0; i < OperandSize(); ++i) {
            auto ir_old_value = this->operands[i];
            if (ir_old_value) {
                ir_old_value->instruction_with_index_list.remove(
                    {Cast<Instruction>(this->shared_from_this()), i});
            }
        }

        this->OperandResize(0);
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Instruction>(this->shared_from_this()));
    }

   protected:
    std::vector<std::shared_ptr<ir::Value>> operands;
};

/// @brief 用于访问结构体的字段, 最终会变为指针偏移的方式, Constant在lowering的时候就做单独处理
class AccessField : virtual public VariableLiked, virtual public Instruction {
   protected:
    AccessField() = default;

   public:
    static std::shared_ptr<AccessField> Create(std::shared_ptr<ir::Value> ir_object,
                                               std::shared_ptr<Field> field) {
        PRAJNA_ASSERT(Is<VariableLiked>(ir_object));
        PRAJNA_ASSERT(field);

        std::shared_ptr<AccessField> self(new AccessField);
        self->OperandResize(1);
        self->object(ir_object);
        self->field = field;
        self->type = field->type;
        self->tag = "FieldAccess";
        return self;
    }

    std::shared_ptr<ir::Value> object() { return this->GetOperand(0); }
    void object(std::shared_ptr<ir::Value> ir_object) { this->SetOperand(0, ir_object); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<AccessField>(this->shared_from_this()));
    }

   public:
    std::shared_ptr<Field> field;
};

class IndexArray : virtual public VariableLiked, virtual public Instruction {
   protected:
    IndexArray() = default;

   public:
    static std::shared_ptr<IndexArray> Create(std::shared_ptr<ir::Value> ir_object,
                                              std::shared_ptr<ir::Value> ir_index) {
        PRAJNA_ASSERT(Is<VariableLiked>(ir_object));
        PRAJNA_ASSERT(ir_index);

        std::shared_ptr<IndexArray> self(new IndexArray);
        self->OperandResize(2);
        self->object(ir_object);
        self->IndexVariable(ir_index);
        if (Is<ArrayType>(ir_object->type)) {
            self->type = Cast<ArrayType>(ir_object->type)->value_type;
        } else {
            self->type = Cast<VectorType>(ir_object->type)->value_type;
        }
        self->tag = "IndexArray";
        return self;
    }

    std::shared_ptr<ir::Value> object() { return this->GetOperand(0); }
    void object(std::shared_ptr<ir::Value> ir_object) { this->SetOperand(0, ir_object); }

    std::shared_ptr<ir::Value> IndexVariable() { return this->GetOperand(1); }
    void IndexVariable(std::shared_ptr<ir::Value> ir_index) { this->SetOperand(1, ir_index); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<IndexArray>(this->shared_from_this()));
    }
};

class IndexPointer : virtual public VariableLiked, virtual public Instruction {
   protected:
    IndexPointer() = default;

   public:
    static std::shared_ptr<IndexPointer> Create(std::shared_ptr<ir::Value> ir_object,
                                                std::shared_ptr<ir::Value> ir_index) {
        PRAJNA_ASSERT(Is<VariableLiked>(ir_object));
        PRAJNA_ASSERT(ir_index);

        std::shared_ptr<IndexPointer> self(new IndexPointer);
        self->OperandResize(2);
        self->object(ir_object);
        self->IndexVariable(ir_index);
        auto ir_pointer_type = Cast<PointerType>(ir_object->type);
        PRAJNA_ASSERT(ir_pointer_type);
        self->type = ir_pointer_type->value_type;
        self->tag = "IndexPointer";
        return self;
    }

    std::shared_ptr<ir::Value> object() {
        PRAJNA_ASSERT(this->OperandSize() == 2);
        return this->GetOperand(0);
    }
    void object(std::shared_ptr<ir::Value> ir_object) {
        PRAJNA_ASSERT(this->OperandSize() == 2);
        this->SetOperand(0, ir_object);
    }

    std::shared_ptr<ir::Value> IndexVariable() { return this->GetOperand(1); }
    void IndexVariable(std::shared_ptr<ir::Value> ir_index) { this->SetOperand(1, ir_index); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<IndexPointer>(this->shared_from_this()));
    }
};

class GetStructElementPointer : public Instruction {
   protected:
    GetStructElementPointer() = default;

   public:
    /// @param ir_pointer 起始指针
    /// @param index 指针偏移下标, 对于结构体来说相当于字段的号数
    static std::shared_ptr<GetStructElementPointer> Create(std::shared_ptr<ir::Value> ir_pointer,
                                                           std::shared_ptr<Field> ir_field) {
        PRAJNA_ASSERT(ir_pointer);
        PRAJNA_ASSERT(ir_field);
        std::shared_ptr<GetStructElementPointer> self(new GetStructElementPointer);
        self->OperandResize(1);
        self->Pointer(ir_pointer);
        self->field = ir_field;
        self->type = PointerType::Create(ir_field->type);
        self->tag = "GetStructElementPointer";
        return self;
    }

    std::shared_ptr<ir::Value> Pointer() { return this->GetOperand(0); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { this->SetOperand(0, ir_pointer); }

    /// @brief  指针偏移下标, 对于结构体来说相当于字段的号数
    std::shared_ptr<Field> field;

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<GetStructElementPointer>(this->shared_from_this()));
    }
};

/// @brief 和llvm::GetElementPtrInst相对应,参数也一致这样更好生成代码,
/// 某种意义上该类的接口有点不太合理
/// @return
class GetArrayElementPointer : public Instruction {
   protected:
    GetArrayElementPointer() = default;

   public:
    /// @param ir_pointer 起始指针
    /// @param index 指针偏移下标, 对于结构体来说相当于字段的号数
    static std::shared_ptr<GetArrayElementPointer> Create(std::shared_ptr<ir::Value> ir_pointer,
                                                          std::shared_ptr<ir::Value> ir_index) {
        PRAJNA_ASSERT(ir_pointer);
        PRAJNA_ASSERT(ir_index);
        std::shared_ptr<GetArrayElementPointer> self(new GetArrayElementPointer);
        self->OperandResize(2);
        self->Pointer(ir_pointer);
        self->IndexVariable(ir_index);
        auto ir_pointer_type = Cast<PointerType>(ir_pointer->type);
        PRAJNA_ASSERT(ir_pointer_type);
        if (Is<ArrayType>(ir_pointer_type->value_type)) {
            self->type =
                PointerType::Create(Cast<ArrayType>(ir_pointer_type->value_type)->value_type);
        } else {
            self->type =
                PointerType::Create(Cast<VectorType>(ir_pointer_type->value_type)->value_type);
        }
        self->tag = "GetArrayElementPointer";
        return self;
    }

    std::shared_ptr<ir::Value> Pointer() { return this->GetOperand(0); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { this->SetOperand(0, ir_pointer); }

    std::shared_ptr<ir::Value> IndexVariable() { return this->GetOperand(1); }
    void IndexVariable(std::shared_ptr<ir::Value> ir_index) { this->SetOperand(1, ir_index); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<GetArrayElementPointer>(this->shared_from_this()));
    }
};

class GetPointerElementPointer : public Instruction {
   protected:
    GetPointerElementPointer() = default;

   public:
    /// @param ir_pointer 起始指针
    /// @param index 指针偏移下标, 对于结构体来说相当于字段的号数
    static std::shared_ptr<GetPointerElementPointer> Create(std::shared_ptr<ir::Value> ir_pointer,
                                                            std::shared_ptr<ir::Value> ir_index) {
        PRAJNA_ASSERT(ir_pointer);
        PRAJNA_ASSERT(ir_index);
        std::shared_ptr<GetPointerElementPointer> self(new GetPointerElementPointer);
        self->OperandResize(2);
        self->Pointer(ir_pointer);
        self->IndexVariable(ir_index);
        auto ir_pointer_type = Cast<PointerType>(ir_pointer->type);
        PRAJNA_ASSERT(ir_pointer_type);
        self->type = ir_pointer_type->value_type;
        PRAJNA_ASSERT(Is<PointerType>(self->type));
        self->tag = "GetPointerElementPointer";
        return self;
    }

    std::shared_ptr<ir::Value> Pointer() { return this->GetOperand(0); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { this->SetOperand(0, ir_pointer); }

    std::shared_ptr<ir::Value> IndexVariable() { return this->GetOperand(1); }
    void IndexVariable(std::shared_ptr<ir::Value> ir_index) { this->SetOperand(1, ir_index); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<GetPointerElementPointer>(this->shared_from_this()));
    }
};

/// @brief 解指针, 可以认为是C语言里的*p, 可读写
class DeferencePointer : virtual public VariableLiked, virtual public Instruction {
   protected:
    DeferencePointer() = default;

   public:
    static std::shared_ptr<DeferencePointer> Create(std::shared_ptr<ir::Value> ir_pointer) {
        PRAJNA_ASSERT(ir_pointer);
        std::shared_ptr<DeferencePointer> self(new DeferencePointer);
        self->OperandResize(1);
        self->Pointer(ir_pointer);
        auto ir_pointer_type = Cast<PointerType>(ir_pointer->type);
        PRAJNA_ASSERT(ir_pointer_type);
        self->type = ir_pointer_type->value_type;
        self->tag = "DeferencePointer";
        return self;
    }

    std::shared_ptr<ir::Value> Pointer() { return this->GetOperand(0); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { this->SetOperand(0, ir_pointer); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<DeferencePointer>(this->shared_from_this()));
    }
};

class WriteVariableLiked : public Instruction {
   protected:
    WriteVariableLiked() = default;

   public:
    static std::shared_ptr<WriteVariableLiked> Create(std::shared_ptr<ir::Value> ir_value,
                                                      std::shared_ptr<VariableLiked> ir_variable) {
        PRAJNA_ASSERT(ir_value);
        PRAJNA_ASSERT(ir_variable);
        PRAJNA_ASSERT(ir_value->type == ir_variable->type);

        // 需要利用std::shared_ptr<Insturction>来初始化enable_shared_from_this的weak_ptr指针
        std::shared_ptr<WriteVariableLiked> self(new WriteVariableLiked);
        self->OperandResize(2);
        self->variable(ir_variable);
        self->Value(ir_value);
        self->tag = "WriteVariableLiked";
        return self;
    }

    std::shared_ptr<ir::Value> Value() { return this->GetOperand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) {
        PRAJNA_ASSERT(ir_value);
        return this->SetOperand(0, ir_value);
    }

    std::shared_ptr<VariableLiked> variable() { return Cast<VariableLiked>(this->GetOperand(1)); }
    void variable(std::shared_ptr<VariableLiked> ir_variable) {
        PRAJNA_ASSERT(ir_variable);
        return this->SetOperand(1, ir_variable);
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<WriteVariableLiked>(this->shared_from_this()));
    }
};

class GetAddressOfVariableLiked : public Instruction {
   protected:
    GetAddressOfVariableLiked() = default;

   public:
    static std::shared_ptr<GetAddressOfVariableLiked> Create(
        std::shared_ptr<VariableLiked> ir_variable_liked) {
        PRAJNA_ASSERT(ir_variable_liked);
        std::shared_ptr<GetAddressOfVariableLiked> self(new GetAddressOfVariableLiked);
        self->OperandResize(1);
        self->variable(ir_variable_liked);
        self->type = PointerType::Create(ir_variable_liked->type);
        self->tag = "GetAddressOfVariableLiked";
        return self;
    }

    std::shared_ptr<VariableLiked> variable() { return Cast<VariableLiked>(this->GetOperand(0)); }
    void variable(std::shared_ptr<VariableLiked> ir_variable_liked) {
        this->SetOperand(0, ir_variable_liked);
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<GetAddressOfVariableLiked>(this->shared_from_this()));
    }
};

class Alloca : public Instruction {
   protected:
    Alloca() = default;

   public:
    static std::shared_ptr<Alloca> Create(std::shared_ptr<Type> type, std::shared_ptr<Value> length,
                                          int64_t alignment = -1) {
        PRAJNA_ASSERT(type);
        auto self =
            Cast<Alloca>(std::shared_ptr<Instruction>(static_cast<Instruction*>(new Alloca)));
        self->OperandResize(1);
        self->Length(length);
        self->type = PointerType::Create(type);
        self->tag = "Alloca";
        self->alignment = alignment;
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Alloca>(this->shared_from_this()));
    }

    std::shared_ptr<Value> Length() { return this->GetOperand(0); }
    void Length(std::shared_ptr<Value> ir_length) { this->SetOperand(0, ir_length); }

    int64_t alignment = -1;  // -1表示不设置
};

class GlobalVariable;

/**
 * @note 和llvm::GlobalVariable有一定的对应关系
 */
class GlobalAlloca : public Instruction {
   protected:
    GlobalAlloca() = default;

   public:
    static std::shared_ptr<GlobalAlloca> Create(std::shared_ptr<Type> type) {
        PRAJNA_ASSERT(type);
        std::shared_ptr<GlobalAlloca> self(new GlobalAlloca);
        self->OperandResize(0);
        self->type = PointerType::Create(type);
        self->tag = "GlobalAlloca";
        return self;
    }

    void Detach() override {
        Value::Detach();
        this->parent.reset();
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<GlobalAlloca>(this->shared_from_this()));
    }

   public:
    bool is_external = false;
    uint32_t address_space = 0;
    // std::shared_ptr<GlobalVariable> link_to_global_variable = nullptr;
};

class LoadPointer : public Instruction {
   protected:
    LoadPointer() = default;

   public:
    // 讲_type设置为VoidType而不是nullptr,
    // 因为设置为nullptr的话,后续对type()的使用都得先判断是否为nullptr,并不方便
    static std::shared_ptr<LoadPointer> Create(std::shared_ptr<ir::Value> pointer) {
        PRAJNA_ASSERT(pointer);
        std::shared_ptr<LoadPointer> self(new LoadPointer);
        self->OperandResize(1);
        self->Pointer(pointer);
        auto ir_pointer_type = Cast<PointerType>(pointer->type);
        PRAJNA_ASSERT(ir_pointer_type);
        self->type = ir_pointer_type->value_type;
        self->tag = "LoadPointer";
        return self;
    }

    std::shared_ptr<ir::Value> Pointer() { return this->GetOperand(0); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { return this->SetOperand(0, ir_pointer); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<LoadPointer>(this->shared_from_this()));
    }
};

class StorePointer : public Instruction {
   protected:
    StorePointer() = default;

   public:
    static std::shared_ptr<StorePointer> Create(std::shared_ptr<ir::Value> value,
                                                std::shared_ptr<ir::Value> pointer) {
        PRAJNA_ASSERT(value);
        PRAJNA_ASSERT(pointer);
        auto ir_pointer_type = Cast<PointerType>(pointer->type);
        PRAJNA_ASSERT(ir_pointer_type);
        PRAJNA_ASSERT(ir_pointer_type->value_type == value->type);
        std::shared_ptr<StorePointer> self(new StorePointer);
        self->OperandResize(2);
        self->Pointer(pointer);
        self->Value(value);
        self->tag = "StorePointer";
        return self;
    }

    std::shared_ptr<ir::Value> Value() { return this->GetOperand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) { return this->SetOperand(0, ir_value); }

    std::shared_ptr<ir::Value> Pointer() { return this->GetOperand(1); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { return this->SetOperand(1, ir_pointer); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<StorePointer>(this->shared_from_this()));
    }
};

class Return : public Instruction {
   protected:
    Return() = default;

   public:
    static std::shared_ptr<Return> Create(std::shared_ptr<ir::Value> ir_value) {
        PRAJNA_ASSERT(ir_value);
        auto self =
            Cast<Return>(std::shared_ptr<Instruction>(static_cast<Instruction*>(new Return)));
        self->OperandResize(1);
        self->type = ir_value->type;
        self->Value(ir_value);
        self->tag = "Return";
        return self;
    }

    std::shared_ptr<ir::Value> Value() { return this->GetOperand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) { this->SetOperand(0, ir_value); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Return>(this->shared_from_this()));
    }
};

class BitCast : public Instruction {
   protected:
    BitCast() = default;

   public:
    static std::shared_ptr<BitCast> Create(std::shared_ptr<ir::Value> ir_value,
                                           std::shared_ptr<Type> ir_type) {
        PRAJNA_ASSERT(ir_value);
        PRAJNA_ASSERT(ir_type);
        auto self =
            Cast<BitCast>(std::shared_ptr<Instruction>(static_cast<Instruction*>(new BitCast)));
        self->OperandResize(1);
        self->Value(ir_value);
        self->type = ir_type;
        self->tag = "BitCast";
        return self;
    };

    std::shared_ptr<ir::Value> Value() { return this->GetOperand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) { this->SetOperand(0, ir_value); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<BitCast>(this->shared_from_this()));
    }
};

class Call : public Instruction {
   protected:
    Call() = default;

   public:
    static std::shared_ptr<Call> Create(std::shared_ptr<ir::Value> ir_value,
                                        std::list<std::shared_ptr<ir::Value>> arguments) {
        PRAJNA_ASSERT(ir_value);
        std::shared_ptr<Call> self(new Call);
        self->OperandResize(1 + arguments.size());
        self->Function(ir_value);
        auto ir_function_type = ir_value->GetFunctionType();
        PRAJNA_ASSERT(ir_function_type);
        PRAJNA_ASSERT(ir_function_type->parameter_types.size() == arguments.size());
        int64_t i = 0;
        for (auto [ir_argument, ir_parameter_type] :
             boost::combine(arguments, ir_function_type->parameter_types)) {
            PRAJNA_ASSERT(ir_argument->type == ir_parameter_type);
            self->Argument(i, ir_argument);
            ++i;
        }

        self->type = ir_function_type->return_type;
        self->tag = "Call";
        return self;
    }

    std::shared_ptr<ir::Value> Function() { return this->GetOperand(0); }
    void Function(std::shared_ptr<ir::Value> ir_value) { this->SetOperand(0, ir_value); }

    std::shared_ptr<ir::Value> Argument(int64_t i) { return this->GetOperand(1 + i); }
    void Argument(int64_t i, std::shared_ptr<ir::Value> ir_argument) {
        this->SetOperand(i + 1, ir_argument);
    }

    int64_t ArgumentSize() { return this->OperandSize() - 1; }

    auto Arguments() {
        return std::ranges::subrange(this->operands.begin() + 1, this->operands.end());
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Call>(this->shared_from_this()));
    }
};

class Select : public Instruction {
   protected:
    Select() = default;

   public:
    static std::shared_ptr<Select> Create(std::shared_ptr<ir::Value> ir_condition,
                                          std::shared_ptr<ir::Value> ir_true_value,
                                          std::shared_ptr<ir::Value> ir_false_value) {
        PRAJNA_ASSERT(ir_condition);
        PRAJNA_ASSERT(ir_true_value);
        PRAJNA_ASSERT(ir_false_value);
        PRAJNA_ASSERT(ir_true_value->type == ir_false_value->type);
        auto condition_type = ir_condition->type;
        PRAJNA_VERIFY(Is<BoolType>(condition_type) ||
                      (Is<VectorType>(condition_type) &&
                       Is<BoolType>(Cast<VectorType>(condition_type)->value_type)));

        std::shared_ptr<Select> self(new Select);
        self->OperandResize(3);
        self->Condition(ir_condition);
        self->TrueValue(ir_true_value);
        self->FalseValue(ir_false_value);
        self->type = ir_true_value->type;
        self->tag = "Select";
        return self;
    }

    std::shared_ptr<ir::Value> Condition() { return this->GetOperand(0); }
    void Condition(std::shared_ptr<ir::Value> ir_condition) { this->SetOperand(0, ir_condition); }

    std::shared_ptr<ir::Value> TrueValue() { return this->GetOperand(1); }
    void TrueValue(std::shared_ptr<ir::Value> ir_true_value) { this->SetOperand(1, ir_true_value); }

    std::shared_ptr<ir::Value> FalseValue() { return this->GetOperand(2); }
    void FalseValue(std::shared_ptr<ir::Value> ir_false_value) {
        this->SetOperand(2, ir_false_value);
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Select>(this->shared_from_this()));
    }
};

namespace internal {

class ConditionBranch : public Instruction {
   protected:
    ConditionBranch() = default;

   public:
    static std::shared_ptr<ConditionBranch> Create(std::shared_ptr<ir::Value> condition,
                                                   std::shared_ptr<Block> ir_true_block,
                                                   std::shared_ptr<Block> ir_false_block) {
        PRAJNA_ASSERT(condition);
        PRAJNA_ASSERT(ir_true_block);
        PRAJNA_ASSERT(ir_false_block);
        std::shared_ptr<ConditionBranch> self(new ConditionBranch);
        self->OperandResize(3);
        self->condition(condition);
        self->TrueBlock(ir_true_block);
        self->FalseBlock(ir_false_block);
        self->tag = "ConditionBranch";
        return self;
    }

    std::shared_ptr<ir::Value> Condition() { return this->GetOperand(0); }
    void condition(std::shared_ptr<ir::Value> ir_condition) { this->SetOperand(0, ir_condition); }

    std::shared_ptr<Block> TrueBlock() { return Cast<Block>(this->GetOperand(1)); }
    void TrueBlock(std::shared_ptr<Block> ir_true_block) { this->SetOperand(1, ir_true_block); }

    std::shared_ptr<Block> FalseBlock() { return Cast<Block>(this->GetOperand(2)); }
    void FalseBlock(std::shared_ptr<Block> ir_false_block) { this->SetOperand(2, ir_false_block); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<ConditionBranch>(this->shared_from_this()));
    }
};

class JumpBranch : public Instruction {
   protected:
    JumpBranch() = default;

   public:
    static std::shared_ptr<JumpBranch> Create() {
        std::shared_ptr<JumpBranch> self(new JumpBranch);
        self->OperandResize(1);
        self->tag = "JumpBranch";
        return self;
    }

    static std::shared_ptr<JumpBranch> Create(std::shared_ptr<Block> ir_next) {
        PRAJNA_ASSERT(ir_next);
        std::shared_ptr<JumpBranch> self(new JumpBranch);
        self->OperandResize(1);
        self->NextBlock(ir_next);
        self->tag = "JumpBranch";
        return self;
    }

    std::shared_ptr<Block> NextBlock() { return Cast<Block>(this->GetOperand(0)); }
    void NextBlock(std::shared_ptr<Block> ir_next) { this->SetOperand(0, ir_next); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<JumpBranch>(this->shared_from_this()));
    }
};

/// @brief 仅仅用于辅助IR的转换, 在最终的IR里不应当出现
class Label : public Block {
   protected:
    Label() = default;

   public:
    static std::shared_ptr<Label> Create() {
        std::shared_ptr<Label> self(new Label);
        self->tag = "Label";
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Label>(this->shared_from_this()));
    }
};

}  // namespace internal

/// @brief 用于返回函数参数, 由多个Value构成的
class ValueCollection : public Value, public std::list<std::shared_ptr<ir::Value>> {};

class If : public Instruction {
   protected:
    If() = default;

   public:
    static std::shared_ptr<If> Create(std::shared_ptr<ir::Value> ir_condition,
                                      std::shared_ptr<Block> ir_true_block,
                                      std::shared_ptr<Block> ir_false_block) {
        PRAJNA_ASSERT(ir_condition);
        PRAJNA_ASSERT(ir_true_block);
        PRAJNA_ASSERT(ir_false_block);
        std::shared_ptr<If> self(new If);
        self->OperandResize(3);
        self->condition(ir_condition);
        self->TrueBlock(ir_true_block);
        self->FalseBlock(ir_false_block);

        ir_true_block->parent = self->shared_from_this();
        ir_false_block->parent = self->shared_from_this();

        self->tag = "If";
        return self;
    }

    std::shared_ptr<ir::Value> Condition() { return this->GetOperand(0); }
    void condition(std::shared_ptr<ir::Value> ir_condition) { this->SetOperand(0, ir_condition); }

    std::shared_ptr<Block> TrueBlock() { return Cast<Block>(this->GetOperand(1)); }
    void TrueBlock(std::shared_ptr<Block> ir_true_block) {
        ir_true_block->parent = this->shared_from_this();
        this->SetOperand(1, ir_true_block);
    }

    std::shared_ptr<Block> FalseBlock() { return Cast<Block>(this->GetOperand(2)); }
    void FalseBlock(std::shared_ptr<Block> ir_false_block) {
        ir_false_block->parent = this->shared_from_this();
        this->SetOperand(2, ir_false_block);
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<If>(this->shared_from_this()));
    }
};

class While : public Instruction {
   protected:
    While() = default;

   public:
    static std::shared_ptr<While> Create(std::shared_ptr<ir::Value> ir_condition,
                                         std::shared_ptr<Block> ir_condition_block,
                                         std::shared_ptr<Block> ir_loop_block) {
        std::shared_ptr<While> self(new While);
        self->OperandResize(3);
        self->condition(ir_condition);
        self->ConditionBlock(ir_condition_block);
        self->LoopBlock(ir_loop_block);

        ir_condition_block->parent = self->shared_from_this();
        ir_loop_block->parent = self->shared_from_this();

        self->tag = "While";
        return self;
    }

    std::shared_ptr<ir::Value> Condition() { return this->GetOperand(0); }
    void condition(std::shared_ptr<ir::Value> ir_condition) { this->SetOperand(0, ir_condition); }

    /// @brief 用于存放条件表达式的块
    std::shared_ptr<Block> ConditionBlock() { return Cast<Block>(this->GetOperand(1)); }
    void ConditionBlock(std::shared_ptr<Block> ir_condition_block) {
        ir_condition_block->parent = this->shared_from_this();
        this->SetOperand(1, ir_condition_block);
    }

    std::shared_ptr<Block> LoopBlock() { return Cast<Block>(this->GetOperand(2)); }
    void LoopBlock(std::shared_ptr<Block> ir_true_block) {
        ir_true_block->parent = this->shared_from_this();
        this->SetOperand(2, ir_true_block);
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<While>(this->shared_from_this()));
    }
};

class For : public Instruction {
   protected:
    For() = default;

   public:
    static std::shared_ptr<For> Create(std::shared_ptr<LocalVariable> ir_index,
                                       std::shared_ptr<ir::Value> ir_first,
                                       std::shared_ptr<ir::Value> ir_last,
                                       std::shared_ptr<Block> ir_loop_block) {
        std::shared_ptr<For> self(new For);
        self->OperandResize(4);
        self->IndexVariable(ir_index);
        self->First(ir_first);
        self->Last(ir_last);
        self->LoopBlock(ir_loop_block);

        ir_loop_block->parent = self->shared_from_this();

        self->tag = "For";
        return self;
    }

    std::shared_ptr<LocalVariable> IndexVariable() {
        return Cast<LocalVariable>(this->GetOperand(0));
    }
    void IndexVariable(std::shared_ptr<LocalVariable> ir_index) { this->SetOperand(0, ir_index); }

    std::shared_ptr<ir::Value> First() { return this->GetOperand(1); }
    void First(std::shared_ptr<ir::Value> ir_first) { this->SetOperand(1, ir_first); }

    std::shared_ptr<ir::Value> Last() { return this->GetOperand(2); }
    void Last(std::shared_ptr<ir::Value> ir_last) { this->SetOperand(2, ir_last); }

    std::shared_ptr<Block> LoopBlock() { return Cast<Block>(this->GetOperand(3)); }
    void LoopBlock(std::shared_ptr<Block> ir_loop_block) {
        this->parent = this->shared_from_this();
        this->SetOperand(3, ir_loop_block);
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<For>(this->shared_from_this()));
    }
};

class Break : public Instruction {
   protected:
    Break() = default;

   public:
    static std::shared_ptr<Break> Create(std::shared_ptr<ir::Value> loop) {
        PRAJNA_ASSERT(Is<While>(loop) || Is<For>(loop));
        std::shared_ptr<Break> self(new Break);
        self->OperandResize(1);
        self->loop(loop);
        self->tag = "Break";
        return self;
    }

    std::shared_ptr<ir::Value> Loop() { return this->GetOperand(0); }
    void loop(std::shared_ptr<ir::Value> ir_loop) { this->SetOperand(0, ir_loop); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Break>(this->shared_from_this()));
    }
};

class Continue : public Instruction {
   protected:
    Continue() = default;

   public:
    static std::shared_ptr<Continue> Create(std::shared_ptr<ir::Value> loop) {
        PRAJNA_ASSERT(Is<While>(loop) || Is<For>(loop));
        std::shared_ptr<Continue> self(new Continue);
        self->OperandResize(1);
        self->loop(loop);
        self->tag = "Continue";
        return self;
    }

    std::shared_ptr<ir::Value> Loop() { return this->GetOperand(0); }
    void loop(std::shared_ptr<ir::Value> ir_loop) { this->SetOperand(0, ir_loop); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Continue>(this->shared_from_this()));
    }
};

/**
 * @note 和llvm::GlobalVariable有本质的不同, llvm::GlobalVariable本质是个地址(指针)
 */
class GlobalVariable : public Variable {
   protected:
    GlobalVariable() = default;

   public:
    static std::shared_ptr<GlobalVariable> Create(std::shared_ptr<Type> ir_type) {
        PRAJNA_ASSERT(ir_type);
        std::shared_ptr<GlobalVariable> self(new GlobalVariable);
        self->type = ir_type;
        self->tag = "GlobalVariable";
        return self;
    }

    void Detach() override {
        Value::Detach();
        this->parent.reset();
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<GlobalVariable>(this->shared_from_this()));
    }

   public:
    bool is_external = false;
};

class AccessProperty : public WriteReadAble, virtual public Instruction {
   protected:
    AccessProperty() = default;

   public:
    static std::shared_ptr<AccessProperty> Create(std::shared_ptr<ir::Value> ir_this_pointer,
                                                  std::shared_ptr<ir::Property> ir_property) {
        PRAJNA_ASSERT(ir_this_pointer);
        PRAJNA_ASSERT(ir_property);
        std::shared_ptr<AccessProperty> self(new AccessProperty);
        PRAJNA_ASSERT(ir_property->get_function);
        self->type = ir_property->get_function->function_type->return_type;
        self->OperandResize(1);
        self->ThisPointer(ir_this_pointer);
        self->property = ir_property;
        self->tag = "AccessProperty";

        if (!ir_property->set_function) {
            self->is_writeable = false;
        }

        return self;
    }

    std::shared_ptr<ir::Value> ThisPointer() { return this->GetOperand(0); }
    void ThisPointer(std::shared_ptr<ir::Value> ir_this_pointer) {
        this->SetOperand(0, ir_this_pointer);
    }

    void Arguments(std::list<std::shared_ptr<ir::Value>> ir_arguments) {
        this->OperandResize(1 + ir_arguments.size());
        int64_t i = 1;
        for (auto ir_argument : ir_arguments) {
            this->SetOperand(i, ir_argument);
            ++i;
        }
    }

    std::list<std::shared_ptr<ir::Value>> Arguments() {
        std::list<std::shared_ptr<ir::Value>> ir_arguments;
        for (int64_t i = 1; i < this->OperandSize(); ++i) {
            ir_arguments.push_back(this->GetOperand(i));
        }

        return ir_arguments;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<AccessProperty>(this->shared_from_this()));
    }

   public:
    std::shared_ptr<ir::Property> property = nullptr;
};

class WriteProperty : public Instruction {
   protected:
    WriteProperty() = default;

   public:
    static std::shared_ptr<WriteProperty> Create(
        std::shared_ptr<ir::Value> ir_value, std::shared_ptr<AccessProperty> ir_access_property) {
        PRAJNA_ASSERT(ir_value);
        PRAJNA_ASSERT(ir_access_property);
        PRAJNA_ASSERT(ir_value->type == ir_access_property->type);

        // 需要利用std::shared_ptr<Insturction>来初始化enable_shared_from_this的weak_ptr指针
        std::shared_ptr<WriteProperty> self(new WriteProperty);
        self->OperandResize(2);
        self->property(ir_access_property);
        self->Value(ir_value);
        self->tag = "WriteProperty";
        return self;
    }

    std::shared_ptr<ir::Value> Value() { return this->GetOperand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) { return this->SetOperand(0, ir_value); }

    std::shared_ptr<AccessProperty> property() { return Cast<AccessProperty>(this->GetOperand(1)); }
    void property(std::shared_ptr<AccessProperty> ir_access_property) {
        return this->SetOperand(1, ir_access_property);
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<WriteProperty>(this->shared_from_this()));
    }
};

class ShuffleVector : public Instruction {
   protected:
    ShuffleVector() = default;

   public:
    static std::shared_ptr<ShuffleVector> Create(std::shared_ptr<ir::Value> ir_value,
                                                 std::shared_ptr<ir::Value> ir_mask) {
        PRAJNA_ASSERT(Is<VectorType>(ir_value->type));
        PRAJNA_ASSERT(Is<VectorType>(ir_mask->type));
        std::shared_ptr<ShuffleVector> self(new ShuffleVector);
        auto ir_vector_type = Cast<VectorType>(ir_value->type);
        self->type = ir::VectorType::Create(ir_vector_type->value_type,
                                            Cast<VectorType>(ir_mask->type)->size);
        self->OperandResize(2);
        self->Value(ir_value);
        self->Mask(ir_mask);
        self->tag = "ShuffleVector";
        return self;
    }

    std::shared_ptr<ir::Value> Value() { return this->GetOperand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) { this->SetOperand(0, ir_value); }

    std::shared_ptr<ir::Value> Mask() { return this->GetOperand(1); }
    void Mask(std::shared_ptr<ir::Value> ir_mask) { this->SetOperand(1, ir_mask); }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<ShuffleVector>(this->shared_from_this()));
    }
};

class Module : public Value {
   protected:
    Module() = default;

   public:
    static std::shared_ptr<Module> Create() {
        auto self = std::shared_ptr<Module>(new Module);
        self->modules[Target::nvptx] = std::shared_ptr<Module>(new Module);
        self->modules[Target::amdgpu] = std::shared_ptr<Module>(new Module);
        return self;
    }
    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) {
        interpreter->Visit(Cast<Module>(this->shared_from_this()));
    }

    void AddFunction(std::shared_ptr<Function> ir_function) {
        ir_function->parent = shared_from_this();
        this->functions.push_back(ir_function);
    }

    void RemoveFunction(std::shared_ptr<Function> ir_function) {
        ir_function->parent.reset();
        this->functions.remove(ir_function);
    }

    void AddGlobalVariable(std::shared_ptr<GlobalVariable> ir_global_variable) {
        ir_global_variable->parent = shared_from_this();
        this->global_variables.push_back(ir_global_variable);
    }

    void AddGlobalAlloca(std::shared_ptr<GlobalAlloca> ir_global_alloca) {
        PRAJNA_ASSERT(std::ranges::count_if(this->global_allocas, [&](auto x) {
                          return x->fullname == ir_global_alloca->fullname;
                      }) == 0);
        ir_global_alloca->parent = shared_from_this();
        this->global_allocas.push_back(ir_global_alloca);
    }

    std::list<std::shared_ptr<Function>> functions;
    std::list<std::shared_ptr<GlobalVariable>> global_variables;
    std::list<std::shared_ptr<GlobalAlloca>> global_allocas;
    std::shared_ptr<lowering::SymbolTable> symbol_table = nullptr;
    std::unordered_map<Target, std::shared_ptr<Module>> modules;
    llvm::Module* llvm_module = nullptr;
};

class ValueAny : public Value {
   protected:
    ValueAny() = default;

   public:
    static std::shared_ptr<ValueAny> Create(std::any any) {
        std::shared_ptr<ValueAny> self(new ValueAny);
        self->any = any;
        return self;
    };
    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<ValueAny>(this->shared_from_this()));
    }

    std::any any;
};

class KernelFunctionCall : public Instruction {
   protected:
    KernelFunctionCall() = default;

   public:
    static std::shared_ptr<KernelFunctionCall> Create(
        std::shared_ptr<ir::Value> ir_function_value, std::shared_ptr<ir::Value> ir_grid_shape,
        std::shared_ptr<ir::Value> ir_block_shape,
        std::list<std::shared_ptr<ir::Value>> arguments) {
        PRAJNA_ASSERT(ir_function_value);
        PRAJNA_ASSERT(ir_grid_shape);
        PRAJNA_ASSERT(ir_block_shape);
        std::shared_ptr<KernelFunctionCall> self(new KernelFunctionCall);
        self->OperandResize(3 + arguments.size());
        self->Function(ir_function_value);
        auto ir_function_type = ir_function_value->GetFunctionType();
        PRAJNA_ASSERT(ir_function_type);
        self->GridShape(ir_grid_shape);
        self->BlockShape(ir_block_shape);

        auto iter_parameter_type = ir_function_type->parameter_types.begin();
        int64_t i = 0;
        for (auto iter_argument = arguments.begin(); iter_argument != arguments.end();
             ++iter_argument, ++iter_parameter_type, ++i) {
            PRAJNA_ASSERT(*iter_parameter_type == (*iter_argument)->type);
            self->Argument(i, *iter_argument);
        }

        self->type = ir_function_type->return_type;
        self->tag = "KernelFunctionCall";
        return self;
    }

    std::shared_ptr<ir::Value> Function() { return this->GetOperand(0); }
    void Function(std::shared_ptr<ir::Value> ir_value) { this->SetOperand(0, ir_value); }

    std::shared_ptr<ir::Value> GridShape() { return this->GetOperand(1); }
    void GridShape(std::shared_ptr<ir::Value> ir_grid_shape) { this->SetOperand(1, ir_grid_shape); }

    std::shared_ptr<ir::Value> BlockShape() { return this->GetOperand(2); }
    void BlockShape(std::shared_ptr<ir::Value> ir_block_shape) {
        this->SetOperand(2, ir_block_shape);
    }

    int64_t ArgumentSize() { return this->OperandSize() - 3; }

    std::shared_ptr<ir::Value> Argument(int64_t i) { return this->GetOperand(3 + i); }
    void Argument(int64_t i, std::shared_ptr<ir::Value> ir_argument) {
        this->SetOperand(i + 3, ir_argument);
    }

    std::list<std::shared_ptr<ir::Value>> Arguments() {
        std::list<std::shared_ptr<ir::Value>> arguments_re;
        for (int64_t i = 0; i < this->OperandSize(); ++i) {
            arguments_re.push_back(this->Argument(i));
        }

        return arguments_re;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<KernelFunctionCall>(this->shared_from_this()));
    }
};

class Closure : public Value {
   protected:
    Closure() = default;

   public:
    static std::shared_ptr<Closure> Create(std::shared_ptr<FunctionType> function_type) {
        std::shared_ptr<Closure> self(new Closure);
        self->function_type = function_type;
        self->type = PointerType::Create(function_type);
        self->tag = "Closure";
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<Closure>(this->shared_from_this()));
    }

   public:
    std::shared_ptr<FunctionType> function_type = nullptr;
    std::list<std::shared_ptr<ir::Value>> parameters;
    std::list<std::shared_ptr<Block>> blocks;
};

class InlineAsm : public Value {
   protected:
    InlineAsm() = default;

   public:
    static std::shared_ptr<InlineAsm> Create(std::shared_ptr<FunctionType> ir_function_type,
                                             std::string str_asm, std::string str_constrains) {
        std::shared_ptr<InlineAsm> self(new InlineAsm);
        self->type = ir_function_type;
        self->str_asm = str_asm;
        self->str_constrains = str_constrains;
        return self;
    }

    void ApplyVisitor(std::shared_ptr<Visitor> interpreter) override {
        interpreter->Visit(Cast<InlineAsm>(this->shared_from_this()));
    }

    std::string str_asm;
    std::string str_constrains;
    bool has_side_effects = false;
    bool is_align_stack = false;
};

inline std::shared_ptr<Function> Value::GetParentFunction() {
    auto ir_parent = Lock(this->parent);
    PRAJNA_ASSERT(ir_parent);
    if (auto ir_function = Cast<Function>(ir_parent)) {
        return ir_function;
    } else {
        return ir_parent->GetParentFunction();
    }
}

inline std::shared_ptr<Block> Value::GetRootBlock() {
    auto ir_parent_function = this->GetParentFunction();
    PRAJNA_ASSERT(ir_parent_function->blocks.size() == 1);
    return ir_parent_function->blocks.front();
}

inline std::list<std::shared_ptr<ir::Value>>::iterator Value::GetBlockIterator() {
    auto ir_parent_block = Cast<ir::Block>(this->GetParentBlock());
    auto iter = std::ranges::find(*ir_parent_block, this->shared_from_this());
    PRAJNA_ASSERT(iter != ir_parent_block->end());
    return iter;
}

inline void Value::Finalize() {
    PRAJNA_ASSERT(this->instruction_with_index_list.size() == 0);
    this->Detach();

    is_finalized = true;
}

inline std::string GetKernelFunctionAddressName(std::shared_ptr<Function> ir_kernel_function) {
    return ConcatFullname(ir_kernel_function->fullname, "kernel_function_address");
}

inline std::shared_ptr<Function> Type::GetMemberFunction(std::string member_function_name) {
    if (this->member_function_dict.count(member_function_name)) {
        return this->member_function_dict[member_function_name];
    }

    for (auto [interface_name, ir_interface] : this->interface_dict) {
        if (!ir_interface) continue;

        for (auto ir_function : ir_interface->functions) {
            if (ir_function->name == member_function_name) {
                return ir_function;
            }
        }
    }

    return nullptr;
}

inline std::shared_ptr<ir::Function> GetFunctionByName(
    std::list<std::shared_ptr<ir::Function>> function_list, std::string name) {
    auto iter_function = std::ranges::find_if(
        function_list, [=](auto ir_function) { return ir_function->name == name; });
    PRAJNA_ASSERT(iter_function != function_list.end(), name);
    return *iter_function;
}

inline bool IsTerminated(std::shared_ptr<ir::Value> ir_value) {
    return Is<Return>(ir_value) || Is<Break>(ir_value) || Is<Continue>(ir_value) ||
           Is<internal::JumpBranch>(ir_value) || Is<internal::ConditionBranch>(ir_value);
}

inline bool IsGlobal(std::shared_ptr<ir::Value> ir_value) {
    if (auto ir_function = Cast<ir::Function>(ir_value)) {
        return true;
    } else if (auto ir_global_variable = Cast<ir::GlobalVariable>(ir_value)) {
        return true;
    } else if (auto ir_global_alloca = Cast<ir::GlobalAlloca>(ir_value)) {
        return true;
    } else {
        return false;
    }
}

}  // namespace prajna::ir
