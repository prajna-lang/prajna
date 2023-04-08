#pragma once

#include <stack>
#include <unordered_map>

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
        auto array_template_struct =
            symbolGet<TemplateStruct>(this->getSymbolByPath(true, {"array", "Array"}));
        PRAJNA_ASSERT(array_template_struct);
        return ir_type->template_struct == array_template_struct;
    }

    bool isPtrType(std::shared_ptr<ir::Type> ir_type) {
        auto ptr_template_struct =
            symbolGet<TemplateStruct>(this->getSymbolByPath(true, {"__ptr", "ptr"}));
        PRAJNA_ASSERT(ptr_template_struct);
        return ir_type->template_struct == ptr_template_struct;
    }

    Symbol getSymbolByPath(bool root_optional, std::vector<std::string> names) {
        PRAJNA_ASSERT(this->symbol_table);
        auto tmp_symbol_table =
            root_optional ? this->symbol_table->rootSymbolTable() : this->symbol_table;
        for (size_t i = 0; i < names.size() - 1; ++i) {
            tmp_symbol_table = symbolGet<SymbolTable>(tmp_symbol_table->get(names[i]));
            if (!tmp_symbol_table) {
                return nullptr;
            }
        }
        return tmp_symbol_table->get(names.back());
    }

    std::shared_ptr<ir::Type> getArrayType(std::shared_ptr<ir::Type> ir_type, size_t length) {
        PRAJNA_ASSERT(symbol_table);

        std::list<Symbol> symbol_template_arguments;
        symbol_template_arguments.push_back(ir_type);
        symbol_template_arguments.push_back(this->getIndexConstant(length));

        auto symbol_array = this->getSymbolByPath(true, {"array", "Array"});
        auto array_template = symbolGet<TemplateStruct>(symbol_array);
        PRAJNA_ASSERT(array_template);
        auto ir_shape3_type =
            array_template->instantiateStructAndImplement(symbol_template_arguments, this->module);
        return ir_shape3_type;
    }

    std::shared_ptr<ir::Type> getShape3Type() {
        return this->getArrayType(this->getIndexType(), 3);
    }

    std::shared_ptr<ir::Type> getPtrType(std::shared_ptr<ir::Type> ir_value_type) {
        auto symbol_ptr = this->getSymbolByPath(true, {"__ptr", "ptr"});
        auto ptr_template = symbolGet<TemplateStruct>(symbol_ptr);
        PRAJNA_ASSERT(ptr_template);
        std::list<Symbol> symbol_template_arguments = {ir_value_type};
        return ptr_template->instantiateStructAndImplement(symbol_template_arguments, this->module);
    }

    std::shared_ptr<ir::Property> getProperty(std::shared_ptr<ir::Type> ir_type, std::string name) {
        auto iter_property_interface =
            std::find_if(RANGE(ir_type->interface_dict), [=](auto key_value) {
                if (!key_value.second) return false;
                auto name_prefix = name + "Property";
                return key_value.second->name.size() >= name_prefix.size() &&
                       key_value.second->name.substr(0, name_prefix.size()) == name_prefix;
            });
        if (iter_property_interface != ir_type->interface_dict.end()) {
            auto ir_property_interface = iter_property_interface->second;
            auto ir_property = ir::Property::create();
            ir_property->get_function =
                ir::getFunctionByName(ir_property_interface->functions, "get");
            ir_property->set_function =
                ir::getFunctionByName(ir_property_interface->functions, "set");
            PRAJNA_ASSERT(ir_property->get_function);
            PRAJNA_ASSERT(ir_property->set_function);
            return ir_property;
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<ir::Property> getLinearIndexProperty(std::shared_ptr<ir::Type> ir_type) {
        auto iter_linear_index_interface =
            std::find_if(RANGE(ir_type->interface_dict), [](auto key_value) {
                if (!key_value.second) return false;

                return key_value.second->name.size() > 12 &&
                       key_value.second->name.substr(0, 12) == "LinearIndex<";
            });
        if (iter_linear_index_interface != ir_type->interface_dict.end()) {
            auto ir_linear_index_interface = iter_linear_index_interface->second;
            auto ir_index_property = ir::Property::create();
            ir_index_property->get_function =
                ir::getFunctionByName(ir_linear_index_interface->functions, "get");
            ir_index_property->set_function =
                ir::getFunctionByName(ir_linear_index_interface->functions, "set");
            return ir_index_property;
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<ir::Property> getArrayIndexProperty(std::shared_ptr<ir::Type> ir_type) {
        auto iter_array_index_interface =
            std::find_if(RANGE(ir_type->interface_dict), [](auto key_value) {
                if (!key_value.second) return false;
                return key_value.second->name.size() > 11 &&
                       key_value.second->name.substr(0, 11) == "ArrayIndex<";
            });
        if (iter_array_index_interface != ir_type->interface_dict.end()) {
            auto ir_array_index_interface = iter_array_index_interface->second;
            auto ir_index_property = ir::Property::create();
            ir_index_property->get_function =
                ir::getFunctionByName(ir_array_index_interface->functions, "get");
            ir_index_property->set_function =
                ir::getFunctionByName(ir_array_index_interface->functions, "set");
            return ir_index_property;
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<ir::WriteProperty> setDim3(std::shared_ptr<ir::Value> ir_shape3, int64_t index,
                                               std::shared_ptr<ir::Value> ir_value) {
        PRAJNA_ASSERT(this->isArrayIndexType(ir_shape3->type));
        auto ir_index_property = this->getLinearIndexProperty(ir_shape3->type);
        PRAJNA_VERIFY(ir_index_property, "linear index property is missing");

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
        PRAJNA_ASSERT(ir_value);
        auto ir_variable_liked = cast<ir::VariableLiked>(ir_value);
        if (ir_variable_liked) {
            return ir_variable_liked;
        } else {
            ir_variable_liked = this->create<ir::LocalVariable>(ir_value->type);
            this->create<ir::WriteVariableLiked>(ir_value, ir_variable_liked);
            return ir_variable_liked;
        }
    }

    void instantiateTypeImplements(std::shared_ptr<ir::Type> ir_type) {
        if (ir_type->template_struct) {
            auto symbol_template_argument_list =
                std::any_cast<std::list<Symbol>>(ir_type->template_arguments_any);
            ir_type->template_struct->instantiateStructAndImplement(symbol_template_argument_list,
                                                                    this->module);
        }
    }

    std::shared_ptr<ir::Function> getMemberFunction(std::shared_ptr<ir::Type> ir_type,
                                                    std::string member_name) {
        this->instantiateTypeImplements(ir_type);

        if (ir_type->function_dict.count(member_name)) {
            return ir_type->function_dict[member_name];
        }

        for (auto [interface_name, ir_interface] : ir_type->interface_dict) {
            if (!ir_interface) continue;

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
        if (auto ir_property = this->getProperty(ir_type, member_name)) {
            auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
            return this->create<ir::AccessProperty>(ir_this_pointer, ir_property);
        }

        return nullptr;
    }

    std::shared_ptr<ir::Call> callMemberFunction(
        std::shared_ptr<ir::Value> ir_object, std::string member_function,
        std::list<std::shared_ptr<ir::Value>> ir_arguments) {
        auto ir_member_function = this->getMemberFunction(ir_object->type, member_function);
        PRAJNA_ASSERT(ir_member_function);
        return this->callMemberFunction(ir_object, ir_member_function, ir_arguments);
    }

    std::shared_ptr<ir::Call> callMemberFunction(
        std::shared_ptr<ir::Value> ir_object, std::shared_ptr<ir::Function> ir_member_function,
        std::list<std::shared_ptr<ir::Value>> ir_arguments) {
        auto ir_variable_liked = this->variableLikedNormalize(ir_object);
        auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        ir_arguments.insert(ir_arguments.begin(), ir_this_pointer);
        return this->create<ir::Call>(ir_member_function, ir_arguments);
    }

    std::shared_ptr<ir::Function> getUnaryOperator(std::shared_ptr<ir::Type> ir_type,
                                                   std::string unary_operator_name) {
        this->instantiateTypeImplements(ir_type);

        std::unordered_map<std::string, std::string> unary_operator_map = {
            {"!", "Not"}, {"+", "Positive"}, {"-", "Negative"}};
        PRAJNA_ASSERT(unary_operator_map.count(unary_operator_name));

        // TODO binary_operator_name的前缀有问题
        auto unary_operator_interface_name =
            unary_operator_map[unary_operator_name] + "<" + ir_type->fullname + ">";
        if (auto ir_interface_implement = ir_type->interface_dict[unary_operator_interface_name]) {
            auto iter_function = std::find_if(
                RANGE(ir_interface_implement->functions),
                [=](std::shared_ptr<ir::Function> ir_function) -> bool {
                    return ir_function->name == unary_operator_map.at(unary_operator_name);
                });
            if (iter_function != ir_interface_implement->functions.end()) {
                return *iter_function;
            }
        }

        return nullptr;
    }

    std::shared_ptr<ir::Function> getBinaryOperator(std::shared_ptr<ir::Type> ir_type,
                                                    std::string binary_operator_name) {
        this->instantiateTypeImplements(ir_type);

        std::unordered_map<std::string, std::string> binary_operator_map = {
            {"==", "Equal"},       {"!=", "NotEqual"}, {"<", "Less"},
            {"<=", "LessOrEqual"}, {">", "Greater"},   {">=", "GreaterOrEqual"},
            {"+", "Add"},          {"-", "Sub"},       {"*", "Multiply"},
            {"/", "Divide"},       {"%", "Remaind"},   {"&", "And"},
            {"|", "Or"},           {"^", "Xor"},       {"!", "Not"}};

        // TODO binary_operator_name的前缀有问题
        if (auto ir_interface_implement =
                ir_type->interface_dict[binary_operator_map[binary_operator_name] + "<" +
                                        ir_type->fullname + ">"]) {
            auto iter_function = std::find_if(
                RANGE(ir_interface_implement->functions),
                [=](std::shared_ptr<ir::Function> ir_function) -> bool {
                    return binary_operator_map.count(binary_operator_name) &&
                           ir_function->name == binary_operator_map.at(binary_operator_name);
                });
            if (iter_function != ir_interface_implement->functions.end()) {
                return *iter_function;
            }
        }

        return nullptr;
    }

    std::shared_ptr<ir::Call> callBinaryOperator(std::shared_ptr<ir::Value> ir_object,
                                                 std::string binary_operator_name,
                                                 std::shared_ptr<ir::Value> ir_operand) {
        std::list<std::shared_ptr<ir::Value>> ir_arguments = {ir_operand};
        auto ir_variable_liked = this->variableLikedNormalize(ir_object);
        auto ir_this_pointer = this->create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        ir_arguments.insert(ir_arguments.begin(), ir_this_pointer);
        auto ir_member_function = getBinaryOperator(ir_object->type, binary_operator_name);
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
        this->symbol_table->setWithAssigningName(ir_function,
                                                 name + this->getCurrentTemplateArgumentsPostify());
        ir_function->parent_module = this->module;
        this->module->functions.push_back(ir_function);
        return ir_function;
    }

    std::shared_ptr<ir::Block> createTopBlockForFunction(
        std::shared_ptr<ir::Function> ir_function) {
        this->function_stack.push(ir_function);
        auto ir_block = ir::Block::create();
        this->function_stack.top()->blocks.push_back(ir_block);
        ir_block->parent_function = this->function_stack.top();
        this->pushBlock(ir_block);
        this->inserter_iterator = ir_block->values.end();
        return ir_block;
    }

    std::string getCurrentTemplateArgumentsPostify() {
        if (this->symbol_template_argument_list_optional) {
            auto re =
                getTemplateArgumentsPostify(this->symbol_template_argument_list_optional.get());
            // 用完之后清空, 否则会影响的下面的函数
            this->symbol_template_argument_list_optional.reset();
            return re;
        } else {
            return "";
        }
    }

    bool isBuildingMemberfunction() { return current_implement_type && !is_static_function; }

    bool isInsideImplement() { return current_implement_type != nullptr; }

   public:
    std::shared_ptr<SymbolTable> symbol_table = nullptr;
    std::shared_ptr<ir::Module> module = nullptr;

    std::stack<std::shared_ptr<ir::Function>> function_stack;

    std::stack<std::shared_ptr<ir::Label>> loop_after_label_stack;
    std::stack<std::shared_ptr<ir::Label>> loop_before_label_stack;
    std::stack<std::shared_ptr<ir::Block>> block_stack;

    ir::Block::iterator inserter_iterator;
    std::function<void(std::shared_ptr<ir::Value>)> create_callback;

    std::shared_ptr<Logger> logger = nullptr;

    std::stack<std::shared_ptr<ir::Type>> instantiating_type_stack;
    /// TODO we should not build function inside a function which is builting.
    std::shared_ptr<ir::Type> current_implement_type = nullptr;
    std::shared_ptr<ir::InterfaceImplement> current_implement_interface = nullptr;
    boost::optional<std::list<Symbol>> symbol_template_argument_list_optional;
    bool is_static_function = false;
};

}  // namespace prajna::lowering
