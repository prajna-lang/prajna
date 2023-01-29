#pragma once

#include <any>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "prajna/assert.hpp"
#include "prajna/helper.hpp"
#include "prajna/named.hpp"

namespace llvm {

class Type;

}

namespace prajna::ir {

const size_t ADDRESS_BITS = 64;

class Function;
struct Field;
struct Interface;

struct Property {
    std::shared_ptr<ir::Function> setter_function;
    std::shared_ptr<ir::Function> getter_function;
};

using Annotations = std::map<std::string, std::vector<std::string>>;

class Type : public Named {
   protected:
    Type() = default;

   public:
    virtual ~Type() {}

   public:
    // @ref https://llvm.org/docs/LangRef.html#langref-datalayout bytes是多少可参阅datalyout的描述
    size_t bytes = 0;
    std::shared_ptr<Function> constructor = nullptr;
    std::shared_ptr<Function> clone_function = nullptr;
    std::shared_ptr<Function> initialize_function = nullptr;
    std::shared_ptr<Function> copy_function = nullptr;
    std::shared_ptr<Function> destroy_function = nullptr;

    std::map<std::string, std::shared_ptr<Function>> static_functions;
    std::map<std::string, std::shared_ptr<Function>> unary_functions;
    std::map<std::string, std::shared_ptr<Function>> binary_functions;
    std::map<std::string, std::shared_ptr<Property>> properties;
    std::map<std::string, std::shared_ptr<Interface>> interfaces;
    std::vector<std::shared_ptr<Field>> fields;

    std::any template_arguments;

    llvm::Type* llvm_type = nullptr;
};

class NullType : public Type {
   protected:
    NullType() = default;

   public:
    static std::shared_ptr<NullType> create() {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_null_type = cast<NullType>(ir_type)) {
                return ir_null_type;
            }
        }

        std::shared_ptr<NullType> self(new NullType);
        // i1 默认为一
        self->bytes = 0;
        self->name = "NullType";
        self->fullname = "NullType";
        global_context.created_types.push_back(self);
        return self;
    }
};

class BoolType : public Type {
   protected:
    BoolType() = default;

   public:
    static std::shared_ptr<BoolType> create() {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_bool_type = cast<BoolType>(ir_type)) {
                return ir_bool_type;
            }
        }

        std::shared_ptr<BoolType> self(new BoolType);
        // i1 默认为一
        self->bytes = 1;
        self->name = "bool";
        self->fullname = "bool";
        global_context.created_types.push_back(self);
        return self;
    }

    // 我们按惯例使用一个字节来表示Bool类型
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
    static std::shared_ptr<FloatType> create(int64_t bits) {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_float_type = cast<FloatType>(ir_type)) {
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

class IntType : public RealNumberType {
   protected:
    IntType() = default;

   public:
    static std::shared_ptr<IntType> create(int64_t bits, bool is_signed) {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_int_type = cast<IntType>(ir_type)) {
                if (ir_int_type->bits == bits && ir_int_type->is_signed == is_signed) {
                    return ir_int_type;
                }
            }
        }

        std::shared_ptr<IntType> self(new IntType);
        self->bits = bits;
        self->bytes = bits / 8;
        self->is_signed = is_signed;
        self->name = std::string(is_signed ? "i" : "u") + std::to_string(bits);
        self->fullname = std::string(is_signed ? "i" : "u") + std::to_string(bits);
        global_context.created_types.push_back(self);
        return self;
    }

   public:
    bool is_signed;
};

class CharType : public Type {
   protected:
    CharType() = default;

   public:
    static std::shared_ptr<CharType> create() {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_char_type = cast<CharType>(ir_type)) {
                return ir_char_type;
            }
        }

        std::shared_ptr<CharType> self(new CharType);
        self->name = "char";
        self->bytes = 1;
        self->fullname = "char";
        global_context.created_types.push_back(self);
        return self;
    }
};

class VoidType : public Type {
   protected:
    VoidType() = default;

   public:
    static std::shared_ptr<VoidType> create() {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_void_type = cast<VoidType>(ir_type)) {
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
};

class UndefType : public Type {
   protected:
    UndefType() = default;

   public:
    static std::shared_ptr<UndefType> create() {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_undef_type = cast<UndefType>(ir_type)) {
                return ir_undef_type;
            }
        }

        std::shared_ptr<UndefType> self(new UndefType);
        self->name = "undef";
        self->bytes = 0;  // 应该是个无效值
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
    static std::shared_ptr<FunctionType> create(
        std::shared_ptr<Type> return_type, std::vector<std::shared_ptr<Type>> ir_argument_types) {
        // @note 不同函数的, 函数类型不应该是用一个指针, 下面的代码更适合判断动态分发的时候使用
        for (auto ir_type : global_context.created_types) {
            if (auto ir_fun_type = cast<FunctionType>(ir_type)) {
                if (ir_fun_type->return_type == return_type &&
                    ir_fun_type->argument_types.size() == ir_argument_types.size() &&
                    std::equal(RANGE(ir_argument_types), ir_fun_type->argument_types.begin())) {
                    return ir_fun_type;
                }
            }
        }

        std::shared_ptr<FunctionType> self(new FunctionType);
        self->return_type = return_type;
        self->argument_types = ir_argument_types;

        self->name = "(";
        for (size_t i = 0; i < self->argument_types.size(); ++i) {
            self->name += self->argument_types[i]->fullname;
            if (i != self->argument_types.size() - 1) {
                self->name += ",";
            }
        }
        self->name += ")";
        self->name += "->";
        self->name += self->return_type->fullname;
        self->fullname = self->name;

        global_context.created_types.push_back(self);
        return self;
    }

   public:
    std::shared_ptr<Type> return_type = nullptr;
    std::vector<std::shared_ptr<Type>> argument_types;

    std::shared_ptr<Function> function = nullptr;
};

class PointerType : public Type {
   protected:
    PointerType() = default;

   public:
    static std::shared_ptr<PointerType> create(std::shared_ptr<Type> value_type) {
        PRAJNA_ASSERT(not is<VoidType>(value_type));

        for (auto ir_type : global_context.created_types) {
            if (auto ir_pointer_type = cast<PointerType>(ir_type)) {
                if (ir_pointer_type->value_type == value_type) {
                    return ir_pointer_type;
                }
            }
        }

        std::shared_ptr<PointerType> self(new PointerType);
        self->value_type = value_type;
        self->bytes = ADDRESS_BITS / 8;
        self->name = value_type->name + "*";
        self->fullname = self->name;
        global_context.created_types.push_back(self);
        return self;
    }

   public:
    std::shared_ptr<Type> value_type;
};

class ArrayType : public Type {
   protected:
    ArrayType() = default;

   public:
    static std::shared_ptr<ArrayType> create(std::shared_ptr<Type> value_type, size_t size) {
        for (auto ir_type : global_context.created_types) {
            if (auto ir_array_type = cast<ArrayType>(ir_type)) {
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
    std::shared_ptr<Type> value_type;
    size_t size;
};

class Field {
   protected:
    Field() = default;

   public:
    static std::shared_ptr<Field> create(std::string name, std::shared_ptr<Type> type,
                                         size_t index) {
        std::shared_ptr<Field> self(new Field);
        self->name = name;
        self->type = type;
        self->index = index;
        return self;
    }

    std::string name;
    std::shared_ptr<Type> type;
    size_t index;
};

class StructType : public Type {
   public:
    StructType() = default;

   public:
    // StructType应该直接用name获取, 其是否相等的判断取决于是否是相同的struct,而不是类型相等
    static std::shared_ptr<StructType> create(std::vector<std::shared_ptr<Field>> fields) {
        // 每次取得都不是同一个类型
        std::shared_ptr<StructType> self(new StructType);
        self->fields = fields;
        global_context.created_types.push_back(self);
        self->name = "PleaseDefineStructTypeName";
        self->fullname = "PleaseDefineStructTypeFullname";
        self->update();

        return self;
    }

    void update() {
        this->bytes = 0;
        for (auto field : fields) {
            this->bytes += field->type->bytes;
        }
    }
};

class Interface : public Named {
   public:
    Interface() = default;

   public:
    static std::shared_ptr<Interface> create() {
        std::shared_ptr<Interface> self(new Interface);

        self->name = "PleaseDefineInterfaceName";
        self->fullname = "PleaseDefineInterafceFullname";

        return self;
    };

   public:
    std::map<std::string, std::shared_ptr<Function>> functions;
};

}  // namespace prajna::ir
