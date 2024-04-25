#pragma once

#include <any>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "prajna/assert.hpp"
#include "prajna/helper.hpp"
#include "prajna/named.hpp"

namespace llvm {

class Type;
class Module;

}  // namespace llvm

namespace prajna::lowering {

class SymbolTable;
class Template;
class TemplateStruct;

}  // namespace prajna::lowering

namespace prajna::ir {

const int64_t ADDRESS_BITS = 64;

class Function;
struct Field;
struct InterfacePrototype;
struct InterfaceImplement;

struct Property {
    std::shared_ptr<ir::Function> set_function;
    std::shared_ptr<ir::Function> get_function;

    static std::shared_ptr<Property> Create() { return std::shared_ptr<Property>(new Property); }
};

using AnnotationDict = std::unordered_map<std::string, std::list<std::string>>;

class Type : public Named {
   protected:
    Type() = default;

   public:
    virtual ~Type() {}

    virtual bool IsFirstClass() { return true; }

    std::shared_ptr<Function> GetMemberFunction(std::string member_function_name);

   public:
    // @ref https://llvm.org/docs/LangRef.html#langref-datalayout bytes是多少可参阅datalyout的描述
    int64_t bytes = 0;

    std::unordered_map<std::string, std::shared_ptr<Function>> member_function_dict;
    std::unordered_map<std::string, std::shared_ptr<Function>> static_function_dict;
    std::unordered_map<std::string, std::any> template_any_dict;
    std::unordered_map<std::string, std::shared_ptr<InterfaceImplement>> interface_dict;
    // fields必须有顺序关系, 故没有使用map
    std::list<std::shared_ptr<Field>> fields;
    // 用于追溯类型是被什么模板实例化的
    std::shared_ptr<lowering::TemplateStruct> template_struct = nullptr;
    std::any template_arguments_any;
    llvm::Type* llvm_type = nullptr;
};

class RealNumberType : public Type {
   protected:
    RealNumberType() = default;

   public:
    int64_t bits = 0;
};

class FloatType : public RealNumberType {
   protected:
    FloatType() = default;

   public:
    static std::shared_ptr<FloatType> Create(int64_t bits) {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_float_type = Cast<FloatType>(ir_type)) {
                if (ir_float_type->bits == bits) {
                    return ir_float_type;
                }
            }
        }

        std::shared_ptr<FloatType> self(new FloatType);
        self->bits = bits;
        self->bytes = bits / 8;
        self->name = "f" + std::to_string(bits);
        self->fullname = "f" + std::to_string(bits);
        global_context.created_types.push_back(self);
        return self;
    }
};

class CharType;
class BoolType;

class IntType : public RealNumberType {
   protected:
    IntType() = default;

   public:
    static std::shared_ptr<IntType> Create(int64_t bits, bool is_signed) {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_int_type = Cast<IntType>(ir_type)) {
                // 不能和char和bool混了
                if (Is<ir::CharType>(ir_type) || Is<ir::BoolType>(ir_type)) {
                    continue;
                }

                if (ir_int_type->bits == bits && ir_int_type->is_signed == is_signed) {
                    return ir_int_type;
                }
            }
        }

        std::shared_ptr<IntType> self(new IntType);
        self->bits = bits;
        self->bytes = (bits + 7) / 8;
        self->is_signed = is_signed;
        self->name = std::string(is_signed ? "i" : "u") + std::to_string(bits);
        self->fullname = std::string(is_signed ? "i" : "u") + std::to_string(bits);
        global_context.created_types.push_back(self);
        return self;
    }

   public:
    bool is_signed = true;
};

class BoolType : public IntType {
   protected:
    BoolType() = default;

   public:
    static std::shared_ptr<BoolType> Create() {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_bool_type = Cast<BoolType>(ir_type)) {
                return ir_bool_type;
            }
        }

        std::shared_ptr<BoolType> self(new BoolType);
        self->is_signed = false;
        // i1 默认为1个字节
        self->bits = 8;
        self->bytes = 1;
        self->name = "bool";
        self->fullname = "bool";
        global_context.created_types.push_back(self);
        return self;
    }

    // 我们按惯例使用一个字节来表示Bool类型
};

class CharType : public IntType {
   protected:
    CharType() = default;

   public:
    static std::shared_ptr<CharType> Create() {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_char_type = Cast<CharType>(ir_type)) {
                return ir_char_type;
            }
        }

        std::shared_ptr<CharType> self(new CharType);
        self->bits = 8;
        self->is_signed = false;
        self->bytes = 1;

        self->name = "char";
        self->fullname = "char";
        global_context.created_types.push_back(self);
        return self;
    }
};

class VoidType : public Type {
   protected:
    VoidType() = default;

   public:
    static std::shared_ptr<VoidType> Create() {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_void_type = Cast<VoidType>(ir_type)) {
                return ir_void_type;
            }
        }

        std::shared_ptr<VoidType> self(new VoidType);
        self->name = "void";
        self->bytes = 0;  // 应该是个无效值
        self->fullname = "void";
        global_context.created_types.push_back(self);
        return self;
    }

    bool IsFirstClass() override { return false; }
};

class UndefType : public Type {
   protected:
    UndefType() = default;

   public:
    static std::shared_ptr<UndefType> Create() {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_undef_type = Cast<UndefType>(ir_type)) {
                return ir_undef_type;
            }
        }

        std::shared_ptr<UndefType> self(new UndefType);
        self->name = "undef";
        self->bytes = 1;  // 应该是个无效值
        self->fullname = "undef";
        global_context.created_types.push_back(self);
        return self;
    }
};

class FunctionType : public Type {
   protected:
    FunctionType() = default;

   public:
    /// @note 考虑函数指针的情况, 同一个函数类型指向不同函数地址是存在且必须的(分发的实现)
    static std::shared_ptr<FunctionType> Create(std::list<std::shared_ptr<Type>> ir_parameter_types,
                                                std::shared_ptr<Type> return_type) {
        // @note 不同函数的, 函数类型不应该是用一个指针, 下面的代码更适合判断动态分发的时候使用
        for (auto ir_type : global_context.created_types) {
            if (auto ir_fun_type = Cast<FunctionType>(ir_type)) {
                if (ir_fun_type->return_type == return_type &&
                    ir_fun_type->parameter_types.size() == ir_parameter_types.size() &&
                    std::equal(RANGE(ir_parameter_types), ir_fun_type->parameter_types.begin())) {
                    return ir_fun_type;
                }
            }
        }

        std::shared_ptr<FunctionType> self(new FunctionType);
        self->return_type = return_type;
        self->parameter_types = ir_parameter_types;

        self->name = "(";
        auto iter = self->parameter_types.begin();
        while (iter != self->parameter_types.end()) {
            PRAJNA_ASSERT((*iter)->IsFirstClass());
            self->name += (*iter)->fullname;
            ++iter;
            if (iter == self->parameter_types.end()) {
                self->name += ")";
                break;
            }
            self->name += ", ";
        }
        self->name += "->";
        self->name += self->return_type->fullname;
        self->fullname = self->name;
        global_context.created_types.push_back(self);
        return self;
    }

    bool IsFirstClass() override { return false; }

   public:
    std::shared_ptr<Type> return_type = nullptr;
    std::list<std::shared_ptr<Type>> parameter_types;
    std::shared_ptr<Function> function = nullptr;
};

class PointerType : public Type {
   protected:
    PointerType() = default;

   public:
    static std::shared_ptr<PointerType> Create(std::shared_ptr<Type> value_type) {
        PRAJNA_ASSERT(!Is<VoidType>(value_type));

        for (auto ir_type : global_context.created_types) {
            if (auto ir_pointer_type = Cast<PointerType>(ir_type)) {
                if (ir_pointer_type->value_type == value_type) {
                    return ir_pointer_type;
                }
            }
        }

        std::shared_ptr<PointerType> self(new PointerType);
        self->value_type = value_type;
        self->bytes = ADDRESS_BITS / 8;
        self->name = value_type->fullname + "*";
        self->fullname = self->name;
        global_context.created_types.push_back(self);
        return self;
    }

   public:
    std::shared_ptr<Type> value_type = nullptr;
};

class ArrayType : public Type {
   protected:
    ArrayType() = default;

   public:
    static std::shared_ptr<ArrayType> Create(std::shared_ptr<Type> value_type, int64_t size) {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_array_type = Cast<ArrayType>(ir_type)) {
                if (ir_array_type->value_type == value_type && ir_array_type->size == size) {
                    return ir_array_type;
                }
            }
        }

        std::shared_ptr<ArrayType> self(new ArrayType);
        self->value_type = value_type;
        self->size = size;
        self->bytes = value_type->bytes * size;
        self->name = value_type->name + "[" + std::to_string(size) + "]";
        self->fullname = self->name;
        global_context.created_types.push_back(self);
        return self;
    }

   public:
    std::shared_ptr<Type> value_type = nullptr;
    int64_t size = 0;
};

class VectorType : public Type {
   protected:
    VectorType() = default;

   public:
    static std::shared_ptr<VectorType> Create(std::shared_ptr<Type> value_type, int64_t size) {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_vectory_type = Cast<VectorType>(ir_type)) {
                if (ir_vectory_type->value_type == value_type && ir_vectory_type->size == size) {
                    return ir_vectory_type;
                }
            }
        }

        std::shared_ptr<VectorType> self(new VectorType);
        self->value_type = value_type;
        self->size = size;
        self->bytes = value_type->bytes * size;
        self->name = value_type->name + "[" + std::to_string(size) + "]";
        self->fullname = self->name;
        global_context.created_types.push_back(self);
        return self;
    }

   public:
    std::shared_ptr<Type> value_type = nullptr;
    int64_t size = 0;
};

class Field {
   protected:
    Field() = default;

   public:
    static std::shared_ptr<Field> Create(std::string name, std::shared_ptr<Type> type) {
        std::shared_ptr<Field> self(new Field);
        self->name = name;
        self->type = type;
        self->index = -1;
        return self;
    }

    std::string name;
    std::shared_ptr<Type> type = nullptr;
    int64_t index = 0;
};

class StructType : public Type {
   public:
    StructType() = default;

   public:
    // StructType应该直接用name获取, 其是否相等的判断取决于是否是相同的struct,而不是类型相等
    static std::shared_ptr<StructType> Create(std::list<std::shared_ptr<Field>> fields) {
        // 每次取得都不是同一个类型
        std::shared_ptr<StructType> self(new StructType);
        self->fields = fields;
        global_context.created_types.push_back(self);
        self->name = "PleaseDefineStructTypeName";
        self->fullname = "PleaseDefineStructTypeFullname";
        self->Update();

        return self;
    }

    static std::shared_ptr<StructType> Create() { return Create({}); }

    void Update() {
        this->bytes = 0;
        int64_t i = 0;
        for (auto& ir_field : this->fields) {
            ir_field->index = i;
            this->bytes += ir_field->type->bytes;
            ++i;
        }
    }

    bool is_declaration = false;
};

class InterfacePrototype : public Named {
   public:
    InterfacePrototype() = default;

   public:
    static std::shared_ptr<InterfacePrototype> Create() {
        std::shared_ptr<InterfacePrototype> self(new InterfacePrototype);

        self->name = "PleaseDefineInterfacePrototypeName";
        self->fullname = "PleaseDefineInterafcePrototypeFullname";

        return self;
    };

   public:
    std::list<std::shared_ptr<Function>> functions;
    std::shared_ptr<StructType> dynamic_type = nullptr;

    bool disable_dynamic = false;

    // 用于追溯模板接口
    std::shared_ptr<lowering::Template> template_interface = nullptr;
    std::any template_arguments;
};

class InterfaceImplement : public Named {
   public:
    InterfaceImplement() = default;

   public:
    static std::shared_ptr<InterfaceImplement> Create() {
        std::shared_ptr<InterfaceImplement> self(new InterfaceImplement);

        self->name = "PleaseDefineInterfaceImplementName";
        self->fullname = "PleaseDefineInterfaceImplementFullname";

        return self;
    };

   public:
    std::list<std::shared_ptr<Function>> functions;
    // 将第一个参数包装为undef指针, 以便用于动态分发
    std::unordered_map<std::shared_ptr<Function>, std::shared_ptr<Function>>
        undef_this_pointer_functions;
    std::shared_ptr<InterfacePrototype> prototype = nullptr;
    std::shared_ptr<Function> dynamic_type_creator = nullptr;
};

class SimdType : public Type {
   protected:
    SimdType() = default;

   public:
    static std::shared_ptr<SimdType> Create(std::shared_ptr<Type> value_type, int64_t size) {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_array_type = Cast<SimdType>(ir_type)) {
                if (ir_array_type->value_type == value_type && ir_array_type->size == size) {
                    return ir_array_type;
                }
            }
        }

        std::shared_ptr<SimdType> self(new SimdType);
        self->value_type = value_type;
        self->size = size;
        self->bytes = value_type->bytes * size;
        self->name = value_type->name + "[" + std::to_string(size) + "]";
        self->fullname = self->name;
        global_context.created_types.push_back(self);
        return self;
    }

   public:
    std::shared_ptr<Type> value_type = nullptr;
    int64_t size = 0;
};

}  // namespace prajna::ir
