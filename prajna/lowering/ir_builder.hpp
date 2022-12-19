#pragma once

#include "prajna/ir/ir.hpp"
#include "prajna/lowering/builtin.hpp"
#include "prajna/lowering/expression_lowering_visitor.hpp"
#include "prajna/lowering/symbol_table.hpp"

namespace prajna::lowering {

/// @brief 负责IR在构建过程中一些琐事的工作, 和记录IR的当前Function, Block等,
/// 并将创建的Value等插入其中
class IrBuilder {
   public:
    IrBuilder(std::shared_ptr<SymbolTable> symbol_table, std::shared_ptr<ir::Module> ir_module) {
        current_block = nullptr;
        // current_block = ir::Block::create();
        current_function = nullptr;
        this->module = ir_module;
        this->symbol_table = symbol_table;
    }

    IrBuilder() : IrBuilder(nullptr, nullptr) {}

    inline std::shared_ptr<ir::Type> getIndexType() { return ir::global_context.index_type; }

    inline std::shared_ptr<ir::ConstantInt> getIndexConstant(int64_t value) {
        auto ir_value = ir::ConstantInt::create(getIndexType(), value);
        this->insert(ir_value);
        return ir_value;
    }

    inline std::shared_ptr<ir::ArrayType> getDim3Type() {
        auto ir_dim3_type = ir::ArrayType::create(getIndexType(), 3);
        return ir_dim3_type;
    }

    inline std::shared_ptr<ir::ConstantArray> getDim3Constant(int64_t v0, int64_t v1, int64_t v2) {
        auto ir_dim3_constant = ir::ConstantArray::create(
            getDim3Type(), {getIndexConstant(v0), getIndexConstant(v1), getIndexConstant(v2)});
        return ir_dim3_constant;
    }

    inline std::shared_ptr<ir::LocalVariable> getDim3Variable() {
        return this->create<ir::LocalVariable>(getDim3Type());
    }

    inline void setDim3(std::shared_ptr<ir::Value> ir_dim3, int64_t index,
                        std::shared_ptr<ir::Value> ir_value) {
        auto ir_index = getIndexConstant(index);
        auto ir_index_array = create<ir::IndexArray>(ir_dim3, ir_index);
        create<ir::WriteVariableLiked>(ir_value, ir_index_array);
    }

    template <typename _Value, typename... _Args>
    std::shared_ptr<_Value> create(_Args &&...__args) {
        auto ir_value = _Value::create(std::forward<_Args>(__args)...);
        static_assert(std::is_base_of<ir::Value, _Value>::value);
        PRAJNA_ASSERT(current_block);
        this->insert(ir_value);
        return ir_value;
    }

    void insert(std::shared_ptr<ir::Value> ir_value) {
        current_block->insert(this->inserter_iterator, ir_value);
        if (this->create_callback) {
            this->create_callback(ir_value);
        }
    }

    std::shared_ptr<ir::VariableLiked> variableLikedNormalize(std::shared_ptr<ir::Value> ir_value) {
        auto ir_variable_liked = cast<ir::VariableLiked>(ir_value);
        if (ir_variable_liked) {
            return ir_variable_liked;
        } else {
            ir_variable_liked = this->create<ir::LocalVariable>(ir_value->type);
            this->create<ir::WriteVariableLiked>(ir_value, ir_variable_liked);
            return ir_variable_liked;
        }
    }

    std::shared_ptr<ir::Call> callMemberFunction(
        std::shared_ptr<ir::Value> ir_object, std::string member_function,
        std::vector<std::shared_ptr<ir::Value>> ir_arguments) {
        auto ir_variable_liked = this->variableLikedNormalize(ir_object);
        auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        ir_arguments.insert(ir_arguments.begin(), ir_this_pointer);
        auto ir_member_function = ir_object->type->member_functions[member_function];
        PRAJNA_ASSERT(ir_member_function);
        return this->create<ir::Call>(ir_member_function, ir_arguments);
    }

    std::shared_ptr<ir::Call> callBinaryOeprator(
        std::shared_ptr<ir::Value> ir_object, std::string binary_operator,
        std::vector<std::shared_ptr<ir::Value>> ir_arguments) {
        auto ir_variable_liked = this->variableLikedNormalize(ir_object);
        auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        ir_arguments.insert(ir_arguments.begin(), ir_this_pointer);
        auto ir_member_function = ir_object->type->binary_functions[binary_operator];
        PRAJNA_ASSERT(ir_member_function);
        return this->create<ir::Call>(ir_member_function, ir_arguments);
    }

    bool isArrayType(std::shared_ptr<ir::Type> ir_type) {
        return ir_type->fullname.size() > 13 && ir_type->fullname.substr(0, 13) == "::core::Array";
    }

    void pushSymbolTable() {
        auto symbol_new_table = SymbolTable::create(symbol_table);
        symbol_table = symbol_new_table;
    }

    void popSymbolTable() { symbol_table = symbol_table->parent_symbol_table; }

    void pushBlock() {
        auto ir_new_block = ir::Block::create();
        if (current_block) {
            current_block->values.push_back(ir_new_block);
            ir_new_block->parent_block = current_block;
        }

        current_block = ir_new_block;
        inserter_iterator = current_block->values.end();
    }

    void popBlock() {
        if (current_block) {
            current_block = current_block->parent_block;
            inserter_iterator = current_block->values.end();
        };
    }

    void pushBlock(std::shared_ptr<ir::Block> ir_block) {
        ir_block->parent_block = current_block;
        current_block = ir_block;
        inserter_iterator = current_block->values.end();
    }

    void popBlock(std::shared_ptr<ir::Block> ir_block) {
        PRAJNA_ASSERT(ir_block == current_block);
        current_block = current_block->parent_block;
        inserter_iterator = current_block->values.end();
        ir_block->parent_block = nullptr;
    }

    void pushSymbolTableAndBlock() {
        this->pushSymbolTable();
        this->pushBlock();
    }

    void popSymbolTableAndBlock() {
        this->popSymbolTable();
        this->popBlock();
    }

   public:
    std::shared_ptr<SymbolTable> symbol_table = nullptr;
    std::shared_ptr<ir::Block> current_block = nullptr;
    std::shared_ptr<ir::Function> current_function = nullptr;
    std::shared_ptr<ir::Module> module = nullptr;
    std::shared_ptr<ir::Type> ir_return_type = nullptr;

    ir::Block::iterator inserter_iterator;

    std::function<void(std::shared_ptr<ir::Value>)> create_callback;

    bool enable_raw_array = true;
};

}  // namespace prajna::lowering
