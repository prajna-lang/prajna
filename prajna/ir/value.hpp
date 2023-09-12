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
#include "prajna/ir/cloner.hpp"
#include "prajna/ir/global_context.h"
#include "prajna/ir/type.hpp"

namespace prajna::ir {

enum struct Target { none, host, nvptx };

inline std::string TargetToString(Target ir_target) {
    switch (ir_target) {
        case Target::none:
            PRAJNA_UNREACHABLE;
            return "";
        case Target::host:
            return "host";
        case Target::nvptx:
            return "nvptx";
    }

    return "";
}

inline Target StringToTarget(std::string str_target) {
    if (str_target == "host") return Target::host;
    if (str_target == "nvptx") return Target::nvptx;

    PRAJNA_UNREACHABLE;
    return Target::none;
}

class Instruction;

struct InstructionAndOperandIndex {
    std::shared_ptr<Instruction> instruction;
    size_t operand_index;
};

inline bool operator==(prajna::ir::InstructionAndOperandIndex lhs,
                       prajna::ir::InstructionAndOperandIndex rhs) {
    return lhs.instruction == rhs.instruction && lhs.operand_index == rhs.operand_index;
}

}  // namespace prajna::ir

template <>
struct std::hash<prajna::ir::InstructionAndOperandIndex> {
    std::size_t operator()(prajna::ir::InstructionAndOperandIndex inst_with_idx) const noexcept {
        std::size_t h1 =
            std::hash<std::shared_ptr<prajna::ir::Instruction>>{}(inst_with_idx.instruction);
        std::size_t h2 = std::hash<size_t>{}(inst_with_idx.operand_index);
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

        this->parent_block = nullptr;
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
        this->parent_block = nullptr;
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

    bool IsFunction() { return GetFunctionType() != nullptr; }

    virtual std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) {
        PRAJNA_UNIMPLEMENT;
        return nullptr;
    }

   private:
    bool is_finalized = false;

   public:
    std::shared_ptr<Type> type = nullptr;
    std::unordered_map<std::string, std::list<std::string>> annotation_dict;
    std::shared_ptr<Block> parent_block = nullptr;
    std::list<InstructionAndOperandIndex> instruction_with_index_list;
    ast::SourceLocation source_location;
    llvm::Value* llvm_value = nullptr;
    // 用于方便调试, 否则无法有效辨别他们
    std::string tag = "";
    std::shared_ptr<Function> parent_function = nullptr;
    // 用于判断是否是closure, 用于辅助closure的生成
    bool is_closure = false;
    bool is_global = false;
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

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<VoidValue> ir_new(new VoidValue(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        return ir_new;
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

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<Parameter> ir_new(new Parameter(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        return ir_new;
    }
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

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<ConstantBool> ir_new(new ConstantBool(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        return ir_new;
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

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<ConstantInt> ir_new(new ConstantInt(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        return ir_new;
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

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<ConstantFloat> ir_new(new ConstantFloat(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        return ir_new;
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

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<ConstantChar> ir_new(new ConstantChar(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        return ir_new;
    }

   public:
    char value;
};

class ConstantNull : public Constant {
   protected:
    ConstantNull() = default;

   public:
    static std::shared_ptr<ConstantNull> Create() {
        std::shared_ptr<ConstantNull> self(new ConstantNull);
        self->type = PointerType::Create(nullptr);
        self->tag = "ConstantNull";
        return self;
    }
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

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::list<std::shared_ptr<Constant>> new_initialize_constants(
            this->initialize_constants.size());
        std::transform(
            RANGE(this->initialize_constants), new_initialize_constants.begin(),
            [=](auto ir_constant) {
                PRAJNA_ASSERT(
                    function_cloner->value_dict[ir_constant]);  // constant应该在前面就处理过;
                return Cast<Constant>(function_cloner->value_dict[ir_constant]);
            });

        std::shared_ptr<ConstantArray> ir_new =
            ConstantArray::Create(Cast<ArrayType>(this->type), new_initialize_constants);
        function_cloner->value_dict[shared_from_this()] = ir_new;
        return ir_new;
    }

    std::list<std::shared_ptr<Constant>> initialize_constants;
};

/// @brief  和高级语言里的块是对应的
class Block : public Value {
   protected:
    Block() = default;

   public:
    using iterator = std::list<std::shared_ptr<ir::Value>>::iterator;

    static std::shared_ptr<Block> Create() {
        std::shared_ptr<Block> self(new Block);
        auto p = self->shared_from_this();
        self->type = nullptr;
        self->tag = "Block";
        return self;
    }

    iterator insert(iterator iter, std::shared_ptr<ir::Value> ir_value) {
        ir_value->parent_block = Cast<Block>(this->shared_from_this());
        return this->values.insert(iter, ir_value);
    }

    iterator find(std::shared_ptr<ir::Value> ir_value) {
        PRAJNA_ASSERT(ir_value->parent_block.get() == this);
        return std::find(RANGE(this->values), ir_value);
    }

    iterator erase(iterator iter) {
        PRAJNA_ASSERT((*iter) && (*iter)->parent_block == shared_from_this());
        (*iter)->parent_block = nullptr;
        return this->values.erase(iter);
    }

    void remove(std::shared_ptr<ir::Value> ir_value) {
        ir_value->parent_block = nullptr;
        this->values.erase(std::find(RANGE(this->values), ir_value));
    }

    void PushFront(std::shared_ptr<ir::Value> ir_value) {
        ir_value->parent_block = Cast<Block>(this->shared_from_this());
        this->values.push_front(ir_value);
    }

    void PushBack(std::shared_ptr<ir::Value> ir_value) {
        ir_value->parent_block = Cast<Block>(this->shared_from_this());
        this->values.push_back(ir_value);
    }

    void Finalize() override {
        Value::Finalize();
        this->parent_function = nullptr;
        this->values.clear();
    }

    void Detach() override {
        Value::Detach();
        this->parent_function = nullptr;
    }

    virtual std::shared_ptr<ir::Value> Clone(
        std::shared_ptr<FunctionCloner> function_cloner) override;

   public:
    std::list<std::shared_ptr<ir::Value>> values;
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
        self->is_global = true;
        return self;
    }

    void Update() {
        // @warning 事实上llvm::Function是一个指针类型
        this->parameters.resize(function_type->parameter_types.size());
        this->type = PointerType::Create(function_type);
        std::transform(RANGE(function_type->parameter_types), this->parameters.begin(),
                       [=](std::shared_ptr<Type> ir_argument_type) {
                           auto ir_parameter = Parameter::Create(ir_argument_type);
                           ir_parameter->parent_function =
                               Cast<ir::Function>(this->shared_from_this());
                           return ir_parameter;
                       });
    }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override;

    void Finalize() override {
        Value::Finalize();
        this->parent_module = nullptr;
        this->function_type = nullptr;
        this->parameters.clear();
        this->blocks.clear();
    }

    bool IsDeclaration() { return this->blocks.empty(); }

   public:
    std::shared_ptr<FunctionType> function_type = nullptr;
    std::list<std::shared_ptr<ir::Value>> parameters;
    std::list<std::shared_ptr<Block>> blocks;
    std::shared_ptr<Module> parent_module = nullptr;

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

    std::shared_ptr<ir::Value> this_pointer = nullptr;
    std::shared_ptr<Function> function_prototype = nullptr;
};

class WriteReadAble : virtual public Value {
   protected:
    WriteReadAble() = default;

    bool is_readable = true;
    bool is_writeable = true;
};

class VariableLiked : virtual public WriteReadAble {
   protected:
    VariableLiked() = default;
};

class Variable : public VariableLiked {
   protected:
    Variable() = default;
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

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<LocalVariable> ir_new(new LocalVariable(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        return ir_new;
    }
};

/// @brief

class Instruction : virtual public Value {
   protected:
    Instruction() : Instruction(0) {}

    Instruction(size_t operand_size) { this->operands.resize(operand_size); }

   public:
    virtual void OperandResize(size_t size) { return this->operands.resize(size); }

    virtual size_t OperandSize() { return this->operands.size(); }

    virtual std::shared_ptr<ir::Value> operand(size_t i) {
        PRAJNA_ASSERT(this->OperandSize() > i);
        return this->operands[i];
    };

    virtual void operand(size_t i, std::shared_ptr<ir::Value> ir_value) {
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

        for (size_t i = 0; i < OperandSize(); ++i) {
            auto ir_old_value = this->operands[i];
            if (ir_old_value) {
                ir_old_value->instruction_with_index_list.remove(
                    {Cast<Instruction>(this->shared_from_this()), i});
            }
        }

        this->OperandResize(0);
    }

    void CloneOperands(std::shared_ptr<FunctionCloner> function_cloner) {
        for (size_t i = 0; i < operands.size(); ++i) {
            auto ir_old = operands[i];

            if (!function_cloner->value_dict[ir_old]) {
                ir_old->Clone(function_cloner);
            }

            operands[i] = nullptr;  // 置零以避免干扰原来的操作数
            auto ir_new = function_cloner->value_dict[ir_old];
            operand(i, ir_new);
        }
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

    std::shared_ptr<ir::Value> object() { return this->operand(0); }
    void object(std::shared_ptr<ir::Value> ir_object) { this->operand(0, ir_object); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<AccessField> ir_new(new AccessField(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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
        auto ir_array_type = Cast<ArrayType>(ir_object->type);
        PRAJNA_ASSERT(ir_array_type);
        self->type = ir_array_type->value_type;
        self->tag = "IndexArray";
        return self;
    }

    std::shared_ptr<ir::Value> object() { return this->operand(0); }
    void object(std::shared_ptr<ir::Value> ir_object) { this->operand(0, ir_object); }

    std::shared_ptr<ir::Value> IndexVariable() { return this->operand(1); }
    void IndexVariable(std::shared_ptr<ir::Value> ir_index) { this->operand(1, ir_index); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<IndexArray> ir_new(new IndexArray(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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
        return this->operand(0);
    }
    void object(std::shared_ptr<ir::Value> ir_object) {
        PRAJNA_ASSERT(this->OperandSize() == 2);
        this->operand(0, ir_object);
    }

    std::shared_ptr<ir::Value> IndexVariable() { return this->operand(1); }
    void IndexVariable(std::shared_ptr<ir::Value> ir_index) { this->operand(1, ir_index); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<IndexPointer> ir_new(new IndexPointer(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Pointer() { return this->operand(0); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { this->operand(0, ir_pointer); }

    /// @brief  指针偏移下标, 对于结构体来说相当于字段的号数
    std::shared_ptr<Field> field;

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<GetStructElementPointer> ir_new(new GetStructElementPointer(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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
        auto ir_array_type = Cast<ArrayType>(ir_pointer_type->value_type);
        PRAJNA_ASSERT(ir_array_type);
        self->type = PointerType::Create(ir_array_type->value_type);
        self->tag = "GetArrayElementPointer";
        return self;
    }

    std::shared_ptr<ir::Value> Pointer() { return this->operand(0); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { this->operand(0, ir_pointer); }

    std::shared_ptr<ir::Value> IndexVariable() { return this->operand(1); }
    void IndexVariable(std::shared_ptr<ir::Value> ir_index) { this->operand(1, ir_index); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<GetArrayElementPointer> ir_new(new GetArrayElementPointer(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Pointer() { return this->operand(0); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { this->operand(0, ir_pointer); }

    std::shared_ptr<ir::Value> IndexVariable() { return this->operand(1); }
    void IndexVariable(std::shared_ptr<ir::Value> ir_index) { this->operand(1, ir_index); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<GetPointerElementPointer> ir_new(new GetPointerElementPointer(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Pointer() { return this->operand(0); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { this->operand(0, ir_pointer); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<DeferencePointer> ir_new(new DeferencePointer(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Value() { return this->operand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) {
        PRAJNA_ASSERT(ir_value);
        return this->operand(0, ir_value);
    }

    std::shared_ptr<VariableLiked> variable() { return Cast<VariableLiked>(this->operand(1)); }
    void variable(std::shared_ptr<VariableLiked> ir_variable) {
        PRAJNA_ASSERT(ir_variable);
        return this->operand(1, ir_variable);
    }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<WriteVariableLiked> ir_new(new WriteVariableLiked(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<VariableLiked> variable() { return Cast<VariableLiked>(this->operand(0)); }
    void variable(std::shared_ptr<VariableLiked> ir_variable_liked) {
        this->operand(0, ir_variable_liked);
    }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<GetAddressOfVariableLiked> ir_new(new GetAddressOfVariableLiked(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
    }
};

class Alloca : public Instruction {
   protected:
    Alloca() = default;

   public:
    static std::shared_ptr<Alloca> Create(std::shared_ptr<Type> type) {
        PRAJNA_ASSERT(type);
        auto self =
            Cast<Alloca>(std::shared_ptr<Instruction>(static_cast<Instruction*>(new Alloca)));
        self->OperandResize(0);
        self->type = PointerType::Create(type);
        self->tag = "Alloca";
        return self;
    }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<Alloca> ir_new(new Alloca(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
    }
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
        self->is_global = true;
        return self;
    }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        // 全局的不拷贝
        function_cloner->value_dict[shared_from_this()] = shared_from_this();
        return shared_from_this();
    }

   public:
    bool is_external = false;
    uint32_t address_space = 0;
    std::shared_ptr<Module> parent_module;
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

    std::shared_ptr<ir::Value> Pointer() { return this->operand(0); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { return this->operand(0, ir_pointer); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<LoadPointer> ir_new(new LoadPointer(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Value() { return this->operand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) { return this->operand(0, ir_value); }

    std::shared_ptr<ir::Value> Pointer() { return this->operand(1); }
    void Pointer(std::shared_ptr<ir::Value> ir_pointer) { return this->operand(1, ir_pointer); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<StorePointer> ir_new(new StorePointer(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Value() { return this->operand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) { this->operand(0, ir_value); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<Return> ir_new(new Return(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Value() { return this->operand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) { this->operand(0, ir_value); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<BitCast> ir_new(new BitCast(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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
        size_t i = 0;
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

    static std::shared_ptr<Call> Create(std::shared_ptr<ir::Value> ir_value,
                                        std::shared_ptr<ir::Value> ir_argument) {
        return Create(ir_value, std::list<std::shared_ptr<ir::Value>>{ir_argument});
    }

    static std::shared_ptr<Call> Create(std::shared_ptr<ir::Value> ir_value) {
        return Create(ir_value, std::list<std::shared_ptr<ir::Value>>{});
    }

    std::shared_ptr<ir::Value> Function() { return this->operand(0); }
    void Function(std::shared_ptr<ir::Value> ir_value) { this->operand(0, ir_value); }

    std::shared_ptr<ir::Value> Argument(size_t i) { return this->operand(1 + i); }
    void Argument(size_t i, std::shared_ptr<ir::Value> ir_argument) {
        this->operand(i + 1, ir_argument);
    }

    size_t ArgumentSize() { return this->OperandSize() - 1; }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<Call> ir_new(new Call(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
    }
};

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

    std::shared_ptr<ir::Value> Condition() { return this->operand(0); }
    void condition(std::shared_ptr<ir::Value> ir_condition) { this->operand(0, ir_condition); }

    std::shared_ptr<Block> TrueBlock() { return Cast<Block>(this->operand(1)); }
    void TrueBlock(std::shared_ptr<Block> ir_true_block) { this->operand(1, ir_true_block); }

    std::shared_ptr<Block> FalseBlock() { return Cast<Block>(this->operand(2)); }
    void FalseBlock(std::shared_ptr<Block> ir_false_block) { this->operand(2, ir_false_block); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<ConditionBranch> ir_new(new ConditionBranch(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
    }
};

class JumpBranch : public Instruction {
   protected:
    JumpBranch() = default;

   public:
    static std::shared_ptr<JumpBranch> Create(std::shared_ptr<Block> ir_next) {
        PRAJNA_ASSERT(ir_next);
        std::shared_ptr<JumpBranch> self(new JumpBranch);
        self->OperandResize(1);
        self->NextBlock(ir_next);
        self->tag = "JumpBranch";
        return self;
    }

    std::shared_ptr<Block> NextBlock() { return Cast<Block>(this->operand(0)); }
    void NextBlock(std::shared_ptr<Block> ir_next) { this->operand(0, ir_next); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<JumpBranch> ir_new(new JumpBranch(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<Label> ir_new(new Label(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        return ir_new;
    }
};

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
        self->tag = "If";
        return self;
    }

    std::shared_ptr<ir::Value> Condition() { return this->operand(0); }
    void condition(std::shared_ptr<ir::Value> ir_condition) { this->operand(0, ir_condition); }

    std::shared_ptr<Block> TrueBlock() { return Cast<Block>(this->operand(1)); }
    void TrueBlock(std::shared_ptr<Block> ir_true_block) { this->operand(1, ir_true_block); }

    std::shared_ptr<Block> FalseBlock() { return Cast<Block>(this->operand(2)); }
    void FalseBlock(std::shared_ptr<Block> ir_false_block) { this->operand(2, ir_false_block); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<If> ir_new(new If(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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
        self->tag = "While";
        return self;
    }

    std::shared_ptr<ir::Value> Condition() { return this->operand(0); }
    void condition(std::shared_ptr<ir::Value> ir_condition) { this->operand(0, ir_condition); }

    /// @brief 用于存放条件表达式的块
    std::shared_ptr<Block> ConditionBlock() { return Cast<Block>(this->operand(1)); }
    void ConditionBlock(std::shared_ptr<Block> ir_condition_block) {
        this->operand(1, ir_condition_block);
    }

    std::shared_ptr<Block> LoopBlock() { return Cast<Block>(this->operand(2)); }
    void LoopBlock(std::shared_ptr<Block> ir_true_block) { this->operand(2, ir_true_block); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<While> ir_new(new While(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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
        self->tag = "For";
        return self;
    }

    std::shared_ptr<LocalVariable> IndexVariable() { return Cast<LocalVariable>(this->operand(0)); }
    void IndexVariable(std::shared_ptr<LocalVariable> ir_index) { this->operand(0, ir_index); }

    std::shared_ptr<ir::Value> First() { return this->operand(1); }
    void First(std::shared_ptr<ir::Value> ir_first) { this->operand(1, ir_first); }

    std::shared_ptr<ir::Value> Last() { return this->operand(2); }
    void Last(std::shared_ptr<ir::Value> ir_last) { this->operand(2, ir_last); }

    std::shared_ptr<Block> LoopBlock() { return Cast<Block>(this->operand(3)); }
    void LoopBlock(std::shared_ptr<Block> ir_loop_block) { this->operand(3, ir_loop_block); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<For> ir_new(new For(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Loop() { return this->operand(0); }
    void loop(std::shared_ptr<ir::Value> ir_loop) { this->operand(0, ir_loop); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<Break> ir_new(new Break(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Loop() { return this->operand(0); }
    void loop(std::shared_ptr<ir::Value> ir_loop) { this->operand(0, ir_loop); }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<Continue> ir_new(new Continue(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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
        self->is_global = true;
        return self;
    }

   public:
    bool is_external = false;

    std::shared_ptr<Module> parent_module = nullptr;
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

    std::shared_ptr<ir::Value> ThisPointer() { return this->operand(0); }
    void ThisPointer(std::shared_ptr<ir::Value> ir_this_pointer) {
        this->operand(0, ir_this_pointer);
    }

    void Arguments(std::list<std::shared_ptr<ir::Value>> ir_arguments) {
        this->OperandResize(1 + ir_arguments.size());
        size_t i = 1;
        for (auto ir_argument : ir_arguments) {
            this->operand(i, ir_argument);
            ++i;
        }
    }

    std::list<std::shared_ptr<ir::Value>> Arguments() {
        std::list<std::shared_ptr<ir::Value>> ir_arguments;
        for (size_t i = 1; i < this->OperandSize(); ++i) {
            ir_arguments.push_back(this->operand(i));
        }

        return ir_arguments;
    }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<AccessProperty> ir_new(new AccessProperty(*this));
        function_cloner->value_dict[shared_from_this()] = ir_new;
        ir_new->CloneOperands(function_cloner);
        return ir_new;
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

    std::shared_ptr<ir::Value> Value() { return this->operand(0); }
    void Value(std::shared_ptr<ir::Value> ir_value) { return this->operand(0, ir_value); }

    std::shared_ptr<AccessProperty> property() { return Cast<AccessProperty>(this->operand(1)); }
    void property(std::shared_ptr<AccessProperty> ir_access_property) {
        return this->operand(1, ir_access_property);
    }

    std::shared_ptr<ir::Value> Clone(std::shared_ptr<FunctionCloner> function_cloner) override {
        std::shared_ptr<WriteProperty> ir_new(new WriteProperty(*this));
        ir_new->CloneOperands(function_cloner);
        return ir_new;
    }
};

class Module : public Named, public std::enable_shared_from_this<Module> {
   protected:
    Module() = default;

   public:
    static std::shared_ptr<Module> Create() {
        auto self = std::shared_ptr<Module>(new Module);
        self->modules[ir::Target::nvptx] = std::shared_ptr<Module>(new Module);
        return self;
    }

    std::list<std::shared_ptr<Function>> functions;
    std::list<std::shared_ptr<GlobalVariable>> global_variables;
    std::list<std::shared_ptr<GlobalAlloca>> global_allocas;
    std::shared_ptr<lowering::SymbolTable> symbol_table = nullptr;
    std::shared_ptr<Module> parent_module = nullptr;
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
        size_t i = 0;
        for (auto iter_argument = arguments.begin(); iter_argument != arguments.end();
             ++iter_argument, ++iter_parameter_type, ++i) {
            PRAJNA_ASSERT(*iter_parameter_type == (*iter_argument)->type);
            self->Argument(i, *iter_argument);
        }

        self->type = ir_function_type->return_type;
        self->tag = "KernelFunctionCall";
        return self;
    }

    std::shared_ptr<ir::Value> Function() { return this->operand(0); }
    void Function(std::shared_ptr<ir::Value> ir_value) { this->operand(0, ir_value); }

    std::shared_ptr<ir::Value> GridShape() { return this->operand(1); }
    void GridShape(std::shared_ptr<ir::Value> ir_grid_shape) { this->operand(1, ir_grid_shape); }

    std::shared_ptr<ir::Value> BlockShape() { return this->operand(2); }
    void BlockShape(std::shared_ptr<ir::Value> ir_block_shape) { this->operand(2, ir_block_shape); }

    size_t ArgumentSize() { return this->OperandSize() - 3; }

    std::shared_ptr<ir::Value> Argument(size_t i) { return this->operand(3 + i); }
    void Argument(size_t i, std::shared_ptr<ir::Value> ir_argument) {
        this->operand(i + 3, ir_argument);
    }

    std::list<std::shared_ptr<ir::Value>> Arguments() {
        std::list<std::shared_ptr<ir::Value>> arguments_re;
        for (size_t i = 0; i < this->OperandSize(); ++i) {
            arguments_re.push_back(this->Argument(i));
        }

        return arguments_re;
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

   public:
    std::shared_ptr<FunctionType> function_type = nullptr;
    std::list<std::shared_ptr<ir::Value>> parameters;
    std::list<std::shared_ptr<Block>> blocks;
};

inline std::shared_ptr<Function> Value::GetParentFunction() {
    if (this->parent_function) {
        return this->parent_function;
    } else if (this->parent_block) {
        return this->parent_block->GetParentFunction();
    } else {
        PRAJNA_UNREACHABLE;
        return nullptr;
    }
}

inline std::shared_ptr<Block> Value::GetRootBlock() {
    if (!this->parent_block) {
        if (Is<Block>(shared_from_this())) {
            return Cast<Block>(shared_from_this());
        } else {
            return nullptr;
        }
    }
    PRAJNA_ASSERT(this->parent_block);
    auto root = this->parent_block;
    while (root->parent_block) {
        root = root->parent_block;
    }
    return root;
}

inline std::list<std::shared_ptr<ir::Value>>::iterator Value::GetBlockIterator() {
    auto iter = std::find(RANGE(this->parent_block->values), this->shared_from_this());
    PRAJNA_ASSERT(iter != this->parent_block->values.end());
    return iter;
}

inline void Value::Finalize() {
    PRAJNA_ASSERT(this->instruction_with_index_list.size() == 0);
    this->Detach();

    is_finalized = true;
}

inline std::shared_ptr<ir::Value> Block::Clone(std::shared_ptr<FunctionCloner> function_cloner) {
    std::shared_ptr<Block> ir_new(new Block(*this));
    function_cloner->value_dict[shared_from_this()] = ir_new;

    ir_new->values.clear();
    for (auto ir_value : values) {
        // (branch导致)存在递归, 故有的值已被处理, 此外参数也在函数里处理
        if (!function_cloner->value_dict.count(ir_value)) {
            auto ir_new_value = ir_value->Clone(function_cloner);
            function_cloner->value_dict[ir_value] = ir_new_value;
        }

        ir_new->PushBack(function_cloner->value_dict[ir_value]);
    }

    ir_new->parent_function = Cast<Function>(function_cloner->value_dict[this->parent_function]);
    ir_new->parent_block = Cast<Block>(function_cloner->value_dict[this->parent_block]);

    return ir_new;
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
    auto iter_function = std::find_if(RANGE(function_list),
                                      [=](auto ir_function) { return ir_function->name == name; });
    PRAJNA_ASSERT(iter_function != function_list.end(), name);
    return *iter_function;
}

inline std::shared_ptr<ir::Value> Function::Clone(std::shared_ptr<FunctionCloner> function_cloner) {
    if (function_cloner->shallow && function_cloner->has_cloned) {
        function_cloner->value_dict[shared_from_this()] = shared_from_this();
        return shared_from_this();
    }

    if (function_cloner->shallow) {
        function_cloner->has_cloned = true;
    }

    std::shared_ptr<Function> ir_new(new Function(*this));
    function_cloner->value_dict[shared_from_this()] = ir_new;

    ir_new->parent_module = function_cloner->module;
    ir_new->parameters.clear();
    std::transform(RANGE(parameters), std::back_inserter(ir_new->parameters),
                   [=](auto ir_parameter) {
                       auto ir_new_parameter = ir_parameter->Clone(function_cloner);
                       function_cloner->value_dict[ir_parameter] = ir_new_parameter;
                       return ir_new_parameter;
                   });

    // 需要再开头, 因为函数有可能存在递归
    ir_new->blocks.clear();
    for (auto ir_block : blocks) {
        if (!function_cloner->value_dict.count(ir_block)) {
            ir_block->Clone(function_cloner);
        }

        auto ir_new_block = Cast<Block>(function_cloner->value_dict[ir_block]);
        ir_new->blocks.push_back(ir_new_block);
    }

    // 在后面加入module, 否则顺序不对
    function_cloner->module->functions.push_back(ir_new);
    return ir_new;
}

inline bool IsTerminated(std::shared_ptr<ir::Value> ir_value) {
    return Is<Return>(ir_value) || Is<Break>(ir_value) || Is<Continue>(ir_value) ||
           Is<JumpBranch>(ir_value) || Is<ConditionBranch>(ir_value);
}

}  // namespace prajna::ir
