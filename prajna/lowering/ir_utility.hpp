#pragma once

#include "prajna/ir/ir.hpp"
#include "prajna/lowering/builtin.hpp"
#include "prajna/lowering/expression_lowering_visitor.hpp"
#include "prajna/lowering/symbol_table.hpp"

namespace prajna::lowering {

/// @brief 负责IR在构建过程中一些琐事的工作, 和记录IR的当前Function, Block等,
/// 并将创建的Value等插入其中
class IrUtility {
   public:
    IrUtility(std::shared_ptr<SymbolTable> symbol_table, std::shared_ptr<ir::Module> ir_module) {
        ir_current_block = nullptr;
        // ir_current_block = ir::Block::create();
        ir_current_function = nullptr;
        this->module = ir_module;
        this->symbol_table = symbol_table;
    }

    template <typename _Value, typename... _Args>
    std::shared_ptr<_Value> create(_Args&&... __args) {
        auto value = _Value::create(std::forward<_Args>(__args)...);
        static_assert(std::is_base_of<ir::Value, _Value>::value);
        PRAJNA_ASSERT(ir_current_block);
        ir_current_block->values.push_back(value);
        value->parent_block = ir_current_block;
        return value;
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

    void pushSymbolTable() {
        auto symbol_new_table = SymbolTable::create(symbol_table);
        symbol_table = symbol_new_table;
    }

    void popSymbolTable() { symbol_table = symbol_table->parent_symbol_table; }

    void pushBlock() {
        auto ir_new_block = ir::Block::create();
        if (ir_current_block) {
            ir_current_block->values.push_back(ir_new_block);
            ir_new_block->parent_block = ir_current_block;
        }

        ir_current_block = ir_new_block;
    }

    void popBlock() {
        if (ir_current_block) ir_current_block = ir_current_block->parent_block;
    }

    void pushBlock(std::shared_ptr<ir::Block> ir_block) {
        ir_block->parent_block = ir_current_block;
        ir_current_block = ir_block;
    }

    void popBlock(std::shared_ptr<ir::Block> ir_block) {
        PRAJNA_ASSERT(ir_block == ir_current_block);
        ir_current_block = ir_current_block->parent_block;
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

    std::shared_ptr<ir::Value> callMemberFunction(
        std::shared_ptr<ir::Value> ir_object, std::string member_function_name,
        std::vector<std::shared_ptr<ir::Value>> ir_arguments) {
        auto ir_variable_like = this->variableLikedNormalize(ir_object);
        auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_like);
        ir_arguments.insert(ir_arguments.begin(), ir_this_pointer);
        auto ir_member_function = ir_object->type->member_functions[member_function_name];
        PRAJNA_ASSERT(ir_member_function);
        return this->create<ir::Call>(ir_member_function, ir_arguments);
    }

   public:
    std::shared_ptr<SymbolTable> symbol_table = nullptr;
    std::shared_ptr<ir::Block> ir_current_block = nullptr;
    std::shared_ptr<ir::Function> ir_current_function = nullptr;
    std::shared_ptr<ir::Module> module = nullptr;
    std::shared_ptr<ir::Type> ir_return_type = nullptr;

    bool enable_raw_array = true;
};

}  // namespace prajna::lowering
