#pragma once

#include <stack>

#include "prajna/ir/ir.hpp"
#include "prajna/lowering/builtin.hpp"
#include "prajna/lowering/expression_lowering_visitor.hpp"
#include "prajna/lowering/symbol_table.hpp"
#include "prajna/lowering/template.hpp"

namespace prajna::lowering {

/// @brief 负责IR在构建过程中一些琐事的工作, 和记录IR的当前Function, Block等,
/// 并将创建的Value等插入其中
class IrBuilder {
   protected:
    IrBuilder() = default;

   public:
    static std::shared_ptr<IrBuilder> create(std::shared_ptr<SymbolTable> symbol_table,
                                             std::shared_ptr<ir::Module> ir_module,
                                             std::shared_ptr<Logger> logger) {
        std::shared_ptr<IrBuilder> self(new IrBuilder);
        self->current_function = nullptr;
        self->module = ir_module;
        self->symbol_table = symbol_table;
        self->logger = logger;
        return self;
    }

    static std::shared_ptr<IrBuilder> create() { return create(nullptr, nullptr, nullptr); }

    // IrBuilder() : IrBuilder(nullptr, nullptr, nullptr) {}

    std::shared_ptr<ir::Type> getIndexType() { return ir::IntType::create(ir::ADDRESS_BITS, true); }

    std::shared_ptr<ir::ConstantInt> getIndexConstant(int64_t value) {
        auto ir_value = this->create<ir::ConstantInt>(getIndexType(), value);
        this->insert(ir_value);
        return ir_value;
    }

    std::shared_ptr<ir::LocalVariable> cloneValue(std::shared_ptr<ir::Value> ir_value) {
        auto ir_local_variable = this->create<ir::LocalVariable>(ir_value->type);
        this->create<ir::WriteVariableLiked>(ir_value, ir_local_variable);
        return ir_local_variable;
    }

    bool isArrayIndexType(std::shared_ptr<ir::Type> ir_type) {
        return ir_type->fullname.size() > 17 &&
               ir_type->fullname.substr(0, 17) == "::core::Array<i64";
    }

    bool isPtrType(std::shared_ptr<ir::Type> ir_type) {
        return ir_type->fullname.size() > 14 && ir_type->fullname.substr(0, 14) == "::memory::ptr<";
    }

    Symbol getSymbolByPath(bool is_root, std::vector<std::string> names) {
        PRAJNA_ASSERT(this->symbol_table);
        auto tmp_symbol_table =
            is_root ? this->symbol_table->rootSymbolTable() : this->symbol_table;
        for (size_t i = 0; i < names.size() - 1; ++i) {
            tmp_symbol_table = symbolGet<SymbolTable>(tmp_symbol_table->get(names[i]));
            if (!tmp_symbol_table) {
                return nullptr;
            }
        }
        return tmp_symbol_table->get(names.back());
    }

    std::shared_ptr<ir::StructType> getArrayType(std::shared_ptr<ir::Type> ir_type, size_t length) {
        PRAJNA_ASSERT(symbol_table);

        std::list<Symbol> symbol_template_arguments;
        symbol_template_arguments.push_back(ir_type);
        symbol_template_arguments.push_back(this->getIndexConstant(length));

        auto symbol_array = this->getSymbolByPath(true, {"core", "Array"});
        auto array_template = symbolGet<TemplateStruct>(symbol_array);
        PRAJNA_ASSERT(array_template);
        auto ir_shape3_type =
            array_template->getStructInstance(symbol_template_arguments, this->module);
        return ir_shape3_type;
    }

    std::shared_ptr<ir::StructType> getShape3Type() {
        return this->getArrayType(this->getIndexType(), 3);
    }

    std::shared_ptr<ir::StructType> getPtrType(std::shared_ptr<ir::Type> ir_value_type) {
        auto symbol_ptr = this->getSymbolByPath(true, {"memory", "ptr"});
        auto ptr_template = symbolGet<TemplateStruct>(symbol_ptr);
        PRAJNA_ASSERT(ptr_template);
        std::list<Symbol> symbol_template_arguments = {ir_value_type};
        return ptr_template->getStructInstance(symbol_template_arguments, this->module);
    }

    std::shared_ptr<ir::WriteProperty> setDim3(std::shared_ptr<ir::Value> ir_shape3, int64_t index,
                                               std::shared_ptr<ir::Value> ir_value) {
        PRAJNA_ASSERT(this->isArrayIndexType(ir_shape3->type));
        auto ir_index_property = ir_shape3->type->properties["["];
        PRAJNA_VERIFY(ir_index_property, "Array index property is missing");

        auto ir_shape3_variable_liked = this->variableLikedNormalize(ir_shape3);
        auto ir_array_tmp_this_pointer =
            this->create<ir::GetAddressOfVariableLiked>(ir_shape3_variable_liked);
        auto ir_index = this->getIndexConstant(index);
        auto ir_access_property =
            this->create<ir::AccessProperty>(ir_array_tmp_this_pointer, ir_index_property);
        ir_access_property->arguments({ir_index});
        return this->create<ir::WriteProperty>(ir_value, ir_access_property);
    }

    template <typename Value_, typename... Args_>
    std::shared_ptr<Value_> create(Args_&&... __args) {
        auto ir_value = Value_::create(std::forward<Args_>(__args)...);
        static_assert(std::is_base_of<ir::Value, Value_>::value);
        this->insert(ir_value);
        return ir_value;
    }

    void insert(std::shared_ptr<ir::Value> ir_value) {
        currentBlock()->insert(this->inserter_iterator, ir_value);
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

    std::shared_ptr<ir::Function> getMemberFunction(std::shared_ptr<ir::Type> ir_type,
                                                    std::string member_name) {
        if (member_name == "tochar") {
            int a = 0;
        }
        for (auto [interface_name, ir_interface] : ir_type->interfaces) {
            for (auto ir_function : ir_interface->functions) {
                if (ir_function->name == member_name) {
                    return ir_function;
                }
            }
        }

        return nullptr;
    }

    std::shared_ptr<ir::AccessField> accessField(std::shared_ptr<ir::Value> ir_object,
                                                 std::string field_name) {
        auto ir_variable_liked = this->variableLikedNormalize(ir_object);
        auto iter_field = std::find_if(RANGE(ir_object->type->fields),
                                       [=](auto ir_field) { return ir_field->name == field_name; });
        PRAJNA_ASSERT(iter_field != ir_object->type->fields.end());
        return this->create<ir::AccessField>(ir_variable_liked, *iter_field);
    }

    std::shared_ptr<ir::Value> accessMember(std::shared_ptr<ir::Value> ir_object,
                                            std::string member_name) {
        auto ir_type = ir_object->type;

        auto ir_variable_liked = this->variableLikedNormalize(ir_object);
        if (auto member_function = this->getMemberFunction(ir_type, member_name)) {
            auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
            return ir::MemberFunctionWithThisPointer::create(ir_this_pointer, member_function);
        }

        auto iter_field = std::find_if(
            RANGE(ir_type->fields),
            [=](std::shared_ptr<ir::Field> ir_field) { return ir_field->name == member_name; });
        if (iter_field != ir_type->fields.end()) {
            auto ir_field_access = this->create<ir::AccessField>(ir_variable_liked, *iter_field);
            return ir_field_access;
        }

        // 索引property
        if (auto ir_property = ir_type->properties[member_name]) {
            auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
            return this->create<ir::AccessProperty>(ir_this_pointer, ir_property);
        }

        return nullptr;
    }

    std::shared_ptr<ir::Call> callMemberFunction(
        std::shared_ptr<ir::Value> ir_object, std::string member_function,
        std::vector<std::shared_ptr<ir::Value>> ir_arguments) {
        auto ir_variable_liked = this->variableLikedNormalize(ir_object);
        auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        ir_arguments.insert(ir_arguments.begin(), ir_this_pointer);
        auto ir_member_function = this->getMemberFunction(ir_object->type, member_function);
        PRAJNA_ASSERT(ir_member_function);
        return this->create<ir::Call>(ir_member_function, ir_arguments);
    }

    std::shared_ptr<ir::Call> callBinaryOperator(std::shared_ptr<ir::Value> ir_object,
                                                 std::string binary_operator,
                                                 std::shared_ptr<ir::Value> ir_operand) {
        std::vector<std::shared_ptr<ir::Value>> ir_arguments = {ir_operand};
        auto ir_variable_liked = this->variableLikedNormalize(ir_object);
        auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        ir_arguments.insert(ir_arguments.begin(), ir_this_pointer);
        auto ir_member_function = ir_object->type->binary_functions[binary_operator];
        PRAJNA_ASSERT(ir_member_function);
        return this->create<ir::Call>(ir_member_function, ir_arguments);
    }

    void pushSymbolTable() {
        auto symbol_new_table = SymbolTable::create(symbol_table);
        symbol_table = symbol_new_table;
    }

    void popSymbolTable() { symbol_table = symbol_table->parent_symbol_table; }

    void popBlock() {
        PRAJNA_ASSERT(!block_stack.empty());
        block_stack.pop();
        if (!block_stack.empty()) {
            inserter_iterator = currentBlock()->values.end();
        }
    }

    void createAndPushBlock() {
        auto ir_block = ir::Block::create();
        if (!block_stack.empty()) {
            block_stack.top()->pushBack(ir_block);
        }
        block_stack.push(ir_block);
        inserter_iterator = ir_block->values.end();
    }

    void pushBlock(std::shared_ptr<ir::Block> ir_block) {
        block_stack.push(ir_block);
        inserter_iterator = ir_block->values.end();
    }

    std::shared_ptr<ir::Block> currentBlock() {
        PRAJNA_ASSERT(!block_stack.empty());
        return block_stack.top();
    }

    void popSymbolTableAndBlock() {
        this->popSymbolTable();
        this->popBlock();
    }

    void setSymbolWithAssigningName(Symbol symbol, ast::Identifier ast_identifier) {
        if (symbol_table->currentTableHas(ast_identifier)) {
            logger->error("the symbol is defined already", ast_identifier);
        }

        symbol_table->setWithAssigningName(symbol, ast_identifier);
    }

    std::shared_ptr<ir::Function> createFunction(
        std::string name, std::shared_ptr<ir::FunctionType> ir_function_type) {
        auto ir_function = ir::Function::create(ir_function_type);
        this->current_function = ir_function;
        this->symbol_table->setWithAssigningName(ir_function, name);
        ir_function->parent_module = this->module;
        this->module->functions.push_back(ir_function);
        return ir_function;
    }

    std::shared_ptr<ir::Block> createTopBlockForFunction(
        std::shared_ptr<ir::Function> ir_function) {
        this->current_function = ir_function;
        auto ir_block = ir::Block::create();
        this->current_function->blocks.push_back(ir_block);
        ir_block->parent_function = this->current_function;
        this->pushBlock(ir_block);
        this->inserter_iterator = ir_block->values.end();
        return ir_block;
    }

   public:
    std::shared_ptr<SymbolTable> symbol_table = nullptr;
    std::shared_ptr<ir::Function> current_function = nullptr;
    std::shared_ptr<ir::Module> module = nullptr;

    std::shared_ptr<ir::Type> return_type = nullptr;

    std::stack<std::shared_ptr<ir::Label>> loop_after_label_stack;
    std::stack<std::shared_ptr<ir::Label>> loop_before_label_stack;

    ir::Block::iterator inserter_iterator;
    std::function<void(std::shared_ptr<ir::Value>)> create_callback;
    std::shared_ptr<Logger> logger = nullptr;

    std::stack<std::shared_ptr<ir::Block>> block_stack;

    std::shared_ptr<ir::Type> instantiating_type = nullptr;
    ;
};

}  // namespace prajna::lowering
