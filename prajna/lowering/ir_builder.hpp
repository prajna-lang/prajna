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
    static std::shared_ptr<IrBuilder> Create(std::shared_ptr<SymbolTable> symbol_table,
                                             std::shared_ptr<ir::Module> ir_module,
                                             std::shared_ptr<Logger> logger) {
        std::shared_ptr<IrBuilder> self(new IrBuilder);
        self->module = ir_module;
        self->symbol_table = symbol_table;
        self->logger = logger;
        return self;
    }

    static std::shared_ptr<IrBuilder> Create() { return Create(nullptr, nullptr, nullptr); }

    std::shared_ptr<ir::Type> GetI64Type() { return ir::IntType::Create(ir::ADDRESS_BITS, true); }

    std::shared_ptr<ir::ConstantInt> GetIndexConstant(int64_t value) {
        auto ir_value = this->Create<ir::ConstantInt>(GetI64Type(), value);
        return ir_value;
    }

    std::shared_ptr<ir::LocalVariable> CloneValue(std::shared_ptr<ir::Value> ir_value) {
        auto ir_local_variable = this->Create<ir::LocalVariable>(ir_value->type);
        this->Create<ir::WriteVariableLiked>(ir_value, ir_local_variable);
        return ir_local_variable;
    }

    bool IsArrayI64Type(std::shared_ptr<ir::Type> ir_type) {
        auto array_template_struct =
            SymbolGet<TemplateStruct>(this->GetSymbolByPath(false, {"Array"}));
        PRAJNA_ASSERT(array_template_struct);
        auto symbol_list =
            std::any_cast<std::list<lowering::Symbol>>(ir_type->template_arguments_any);
        if (ir_type->template_struct == array_template_struct) {
            return SymbolGet<ir::Value>(symbol_list.front())->type == this->GetI64Type();
        }

        return false;
    }

    bool IsPtrType(std::shared_ptr<ir::Type> ir_type) {
        auto ptr_template_struct = SymbolGet<TemplateStruct>(this->GetSymbolByPath(false, {"Ptr"}));
        PRAJNA_ASSERT(ptr_template_struct);
        return ir_type->template_struct == ptr_template_struct;
    }

    Symbol GetSymbolByPath(bool root_optional, std::vector<std::string> names) {
        PRAJNA_ASSERT(this->symbol_table);
        auto tmp_symbol_table =
            root_optional ? this->symbol_table->RootSymbolTable() : this->symbol_table;
        for (size_t i = 0; i < names.size() - 1; ++i) {
            tmp_symbol_table = SymbolGet<SymbolTable>(tmp_symbol_table->Get(names[i]));
            if (!tmp_symbol_table) {
                return nullptr;
            }
        }
        return tmp_symbol_table->Get(names.back());
    }

    std::shared_ptr<ir::Type> GetArrayType(std::shared_ptr<ir::Type> ir_type, size_t length) {
        PRAJNA_ASSERT(symbol_table);

        std::list<Symbol> symbol_template_arguments;
        symbol_template_arguments.push_back(ir_type);
        symbol_template_arguments.push_back(this->GetIndexConstant(length));

        auto symbol_array = this->GetSymbolByPath(false, {"Array"});
        auto array_template = SymbolGet<TemplateStruct>(symbol_array);
        PRAJNA_ASSERT(array_template);
        auto ir_shape3_type = array_template->Instantiate(symbol_template_arguments, this->module);
        return ir_shape3_type;
    }

    std::shared_ptr<ir::Type> GetShape3Type() { return this->GetArrayType(this->GetI64Type(), 3); }

    std::shared_ptr<ir::Type> GetManagedPtrType(std::shared_ptr<ir::Type> ir_value_type) {
        auto symbol_ptr = this->GetSymbolByPath(false, {"Ptr"});
        auto ptr_template = SymbolGet<TemplateStruct>(symbol_ptr);
        PRAJNA_ASSERT(ptr_template);
        std::list<Symbol> symbol_template_arguments = {ir_value_type};
        return ptr_template->Instantiate(symbol_template_arguments, this->module);
    }

    std::shared_ptr<ir::Property> GetProperty(std::shared_ptr<ir::Type> ir_type,
                                              std::string property_name) {
        auto ir_property = ir::Property::Create();
        ir_property->get_function = this->GetImplementFunction(ir_type, "__get__" + property_name);
        ir_property->set_function = this->GetImplementFunction(ir_type, "__set__" + property_name);
        if (ir_property->get_function || ir_property->set_function) {
            return ir_property;
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<ir::Property> GetLinearIndexProperty(std::shared_ptr<ir::Type> ir_type) {
        InstantiateTypeImplements(ir_type);

        auto ir_index_property = ir::Property::Create();
        ir_index_property->get_function =
            this->GetImplementFunction(ir_type, "__get_linear_index__");
        ir_index_property->set_function =
            this->GetImplementFunction(ir_type, "__set_linear_index__");
        if (ir_index_property->get_function || ir_index_property->set_function) {
            return ir_index_property;
        }

        return nullptr;
    }

    std::shared_ptr<ir::Property> GetArrayIndexProperty(std::shared_ptr<ir::Type> ir_type) {
        InstantiateTypeImplements(ir_type);

        auto ir_index_property = ir::Property::Create();
        ir_index_property->get_function =
            this->GetImplementFunction(ir_type, "__get_array_index__");
        ir_index_property->set_function =
            this->GetImplementFunction(ir_type, "__set_array_index__");
        if (ir_index_property->get_function || ir_index_property->set_function) {
            return ir_index_property;
        }

        return nullptr;
    }

    std::shared_ptr<ir::WriteProperty> SetDim3(std::shared_ptr<ir::Value> ir_shape3, int64_t index,
                                               std::shared_ptr<ir::Value> ir_value) {
        PRAJNA_ASSERT(this->IsArrayI64Type(ir_shape3->type));
        auto ir_index_property = this->GetLinearIndexProperty(ir_shape3->type);
        PRAJNA_VERIFY(ir_index_property, "linear index property is missing");

        auto ir_shape3_variable_liked = this->VariableLikedNormalize(ir_shape3);
        auto ir_array_tmp_this_pointer =
            this->Create<ir::GetAddressOfVariableLiked>(ir_shape3_variable_liked);
        auto ir_index = this->GetIndexConstant(index);
        auto ir_access_property =
            this->Create<ir::AccessProperty>(ir_array_tmp_this_pointer, ir_index_property);
        ir_access_property->Arguments({ir_index});
        return this->Create<ir::WriteProperty>(ir_value, ir_access_property);
    }

    template <typename Value_, typename... Args_>
    std::shared_ptr<Value_> Create(Args_&&... __args) {
        auto ir_value = Value_::Create(std::forward<Args_>(__args)...);

        if (auto ir_return = Cast<ir::Return>(ir_value)) {
            PRAJNA_ASSERT(ir_return->Value()->type ==
                          this->function_stack.top()->function_type->return_type);
        }

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

    std::shared_ptr<ir::VariableLiked> VariableLikedNormalize(std::shared_ptr<ir::Value> ir_value) {
        PRAJNA_ASSERT(ir_value);
        auto ir_variable_liked = Cast<ir::VariableLiked>(ir_value);
        if (ir_variable_liked) {
            return ir_variable_liked;
        } else {
            ir_variable_liked = this->Create<ir::LocalVariable>(ir_value->type);
            ir_variable_liked->annotation_dict["DisableReferenceCount"];
            auto ir_write_variable_liked =
                this->Create<ir::WriteVariableLiked>(ir_value, ir_variable_liked);
            ir_write_variable_liked->annotation_dict["DisableReferenceCount"];

            return ir_variable_liked;
        }
    }

    void InstantiateTypeImplements(std::shared_ptr<ir::Type> ir_type) {
        // module为nullptr时, 不会参与llvm的生成
        if (ir_type->template_struct && this->module) {
            auto symbol_template_argument_list =
                std::any_cast<std::list<Symbol>>(ir_type->template_arguments_any);
            ir_type->template_struct->Instantiate(symbol_template_argument_list, this->module);
        }
    }

    std::shared_ptr<ir::Function> GetImplementFunction(std::shared_ptr<ir::Type> ir_type,
                                                       std::string member_name) {
        this->InstantiateTypeImplements(ir_type);

        return ir_type->GetImplementFunction(member_name);
    }

    std::shared_ptr<ir::Value> GetString(std::string str) {
        // 最后需要补零, 以兼容C的字符串
        auto char_string_size = str.size() + 1;
        auto ir_char_string_type = ir::ArrayType::Create(ir::CharType::Create(), char_string_size);
        std::list<std::shared_ptr<ir::Constant>> ir_inits(ir_char_string_type->size);
        std::transform(RANGE(str), ir_inits.begin(),
                       [=](char value) -> std::shared_ptr<ir::Constant> {
                           return this->Create<ir::ConstantChar>(value);
                       });
        // 末尾补零
        ir_inits.back() = this->Create<ir::ConstantChar>('\0');
        auto ir_c_string_constant = this->Create<ir::ConstantArray>(ir_char_string_type, ir_inits);
        auto ir_c_string_variable = this->VariableLikedNormalize(ir_c_string_constant);
        auto ir_constant_zero = this->GetIndexConstant(0);
        auto ir_c_string_index0 =
            this->Create<ir::IndexArray>(ir_c_string_variable, ir_constant_zero);
        auto ir_c_string_address = this->Create<ir::GetAddressOfVariableLiked>(ir_c_string_index0);
        ast::IdentifierPath ast_identifier_path;
        auto string_symbol = this->GetSymbolByPath(true, {"String"});
        auto string_type = Cast<ir::StructType>(SymbolGet<ir::Type>(string_symbol));
        auto ir_string_from_char_pat = this->GetImplementFunction(string_type, "__from_char_ptr");
        // 内建函数, 无需动态判断调用是否合法, 若使用错误会触发ir::Call里的断言
        return this->Create<ir::Call>(ir_string_from_char_pat,
                                      std::list<std::shared_ptr<ir::Value>>{ir_c_string_address});
    }

    void ExitWithPrintErrorMessage(std::string str) {
        auto ir_str = this->GetString(str);
        auto ir_exit = SymbolGet<ir::Value>(this->GetSymbolByPath(true, {"ExitWithMessage"}));
        PRAJNA_ASSERT(ir_exit);
        this->Create<ir::Call>(ir_exit, std::list<std::shared_ptr<ir::Value>>{ir_str});
    }

    std::shared_ptr<ir::AccessField> AccessField(std::shared_ptr<ir::Value> ir_object,
                                                 std::string field_name) {
        auto ir_variable_liked = this->VariableLikedNormalize(ir_object);
        auto iter_field = std::find_if(RANGE(ir_object->type->fields),
                                       [=](auto ir_field) { return ir_field->name == field_name; });
        PRAJNA_ASSERT(iter_field != ir_object->type->fields.end());
        return this->Create<ir::AccessField>(ir_variable_liked, *iter_field);
    }

    std::shared_ptr<ir::Value> GetAddressOf(std::shared_ptr<ir::Value> ir_object) {
        auto ir_variable_liked = this->VariableLikedNormalize(ir_object);
        auto ir_this_pointer = this->Create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        return ir_this_pointer;
    }

    std::shared_ptr<ir::Value> AccessMember(std::shared_ptr<ir::Value> ir_object,
                                            std::string member_name) {
        auto ir_type = ir_object->type;

        auto ir_variable_liked = this->VariableLikedNormalize(ir_object);
        if (auto member_function = this->GetImplementFunction(ir_type, member_name)) {
            auto ir_this_pointer = this->Create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
            return ir::MemberFunctionWithThisPointer::Create(ir_this_pointer, member_function);
        }
        auto iter_field = std::find_if(
            RANGE(ir_type->fields),
            [=](std::shared_ptr<ir::Field> ir_field) { return ir_field->name == member_name; });
        if (iter_field != ir_type->fields.end()) {
            auto ir_field_access = this->Create<ir::AccessField>(ir_variable_liked, *iter_field);
            return ir_field_access;
        }

        // 索引property
        if (auto ir_property = this->GetProperty(ir_type, member_name)) {
            auto ir_this_pointer = this->Create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
            return this->Create<ir::AccessProperty>(ir_this_pointer, ir_property);
        }

        return nullptr;
    }

    std::shared_ptr<ir::Call> CallMemberFunction(
        std::shared_ptr<ir::Value> ir_object, std::string member_function,
        std::list<std::shared_ptr<ir::Value>> ir_arguments) {
        auto ir_member_function = this->GetImplementFunction(ir_object->type, member_function);
        PRAJNA_ASSERT(ir_member_function);
        return this->CallMemberFunction(ir_object, ir_member_function, ir_arguments);
    }

    std::shared_ptr<ir::Call> CallMemberFunction(
        std::shared_ptr<ir::Value> ir_object, std::shared_ptr<ir::Function> ir_member_function,
        std::list<std::shared_ptr<ir::Value>> ir_arguments) {
        auto ir_variable_liked = this->VariableLikedNormalize(ir_object);
        auto ir_this_pointer = this->Create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        ir_arguments.insert(ir_arguments.begin(), ir_this_pointer);
        return this->Create<ir::Call>(ir_member_function, ir_arguments);
    }

    std::shared_ptr<ir::Function> GetUnaryOperator(std::shared_ptr<ir::Type> ir_type,
                                                   std::string unary_operator_name) {
        this->InstantiateTypeImplements(ir_type);

        std::unordered_map<std::string, std::string> unary_operator_map = {
            {"!", "not"}, {"+", "positive"}, {"-", "negative"}};
        PRAJNA_ASSERT(unary_operator_map.count(unary_operator_name));

        return this->GetImplementFunction(ir_type,
                                          "__" + unary_operator_map[unary_operator_name] + "__");
    }

    std::shared_ptr<ir::Function> GetBinaryOperator(std::shared_ptr<ir::Type> ir_type,
                                                    std::string binary_operator_name) {
        this->InstantiateTypeImplements(ir_type);

        std::unordered_map<std::string, std::string> binary_operator_map = {
            {"==", "equal"},   {"!=", "not_equal"},
            {"<", "less"},     {"<=", "less_or_equal"},
            {">", "greater"},  {">=", "greater_or_equal"},
            {"+", "add"},      {"-", "sub"},
            {"*", "multiply"}, {"/", "divide"},
            {"%", "remaind"},  {"&", "and"},
            {"|", "or"},       {"^", "xor"}};

        return this->GetImplementFunction(ir_type,
                                          "__" + binary_operator_map[binary_operator_name] + "__");
    }

    std::shared_ptr<ir::Call> CallBinaryOperator(std::shared_ptr<ir::Value> ir_object,
                                                 std::string binary_operator_name,
                                                 std::shared_ptr<ir::Value> ir_operand) {
        std::list<std::shared_ptr<ir::Value>> ir_arguments = {ir_operand};
        auto ir_variable_liked = this->VariableLikedNormalize(ir_object);
        auto ir_this_pointer = this->Create<ir::GetAddressOfVariableLiked>(ir_variable_liked);
        ir_arguments.insert(ir_arguments.begin(), ir_this_pointer);
        auto ir_member_function = GetBinaryOperator(ir_object->type, binary_operator_name);
        PRAJNA_ASSERT(ir_member_function);
        return this->Create<ir::Call>(ir_member_function, ir_arguments);
    }

    void PushSymbolTable() {
        auto symbol_new_table = SymbolTable::Create(symbol_table);
        symbol_table = symbol_new_table;
    }

    void PopSymbolTable() { symbol_table = symbol_table->parent_symbol_table; }

    void PopBlock() {
        PRAJNA_ASSERT(!block_stack.empty());
        block_stack.pop();
        if (!block_stack.empty()) {
            inserter_iterator = currentBlock()->values.end();
        }
    }

    void CreateAndPushBlock() {
        auto ir_block = ir::Block::Create();
        if (!block_stack.empty()) {
            block_stack.top()->PushBack(ir_block);
        }
        block_stack.push(ir_block);
        inserter_iterator = ir_block->values.end();
    }

    void PushBlock(std::shared_ptr<ir::Block> ir_block) {
        block_stack.push(ir_block);
        inserter_iterator = ir_block->values.end();
    }

    std::shared_ptr<ir::Block> currentBlock() {
        PRAJNA_ASSERT(!block_stack.empty());
        return block_stack.top();
    }

    void SetSymbol(Symbol symbol, ast::Identifier ast_identifier) {
        if (symbol_table->CurrentTableHas(ast_identifier)) {
            logger->Error(fmt::format("the symbol {} is defined already", ast_identifier),
                          ast_identifier);
        }

        symbol_table->SetWithAssigningName(symbol, ast_identifier);
    }

    void SetSymbolWithTemplateArgumentsPostify(Symbol symbol, ast::Identifier ast_identifier) {
        ast_identifier.append(this->GetCurrentTemplateArgumentsPostify());
        this->SetSymbol(symbol, ast_identifier);
    }

    std::shared_ptr<ir::Function> createFunction(
        ast::Identifier ast_identifier, std::shared_ptr<ir::FunctionType> ir_function_type) {
        auto ir_function = ir::Function::Create(ir_function_type);
        // 函数必须指定名字, 否则其在module里的名字会混乱和重复
        this->SetSymbolWithTemplateArgumentsPostify(ir_function, ast_identifier);
        ir_function->parent_module = this->module;
        this->module->functions.push_back(ir_function);
        return ir_function;
    }

    std::shared_ptr<ir::Block> CreateTopBlockForFunction(
        std::shared_ptr<ir::Function> ir_function) {
        this->function_stack.push(ir_function);
        auto ir_block = ir::Block::Create();
        this->function_stack.top()->blocks.push_back(ir_block);
        ir_block->parent_function = this->function_stack.top();
        this->PushBlock(ir_block);
        this->inserter_iterator = ir_block->values.end();
        return ir_block;
    }

    std::string GetCurrentTemplateArgumentsPostify() {
        if (this->symbol_template_argument_list_optional) {
            auto re =
                GetTemplateArgumentsPostify(this->symbol_template_argument_list_optional.get());
            // 用完之后清空, 否则会影响的下面的函数
            this->symbol_template_argument_list_optional.reset();
            return re;
        } else {
            return "";
        }
    }

    bool IsBuildingMemberfunction() { return current_implement_type && !is_static_function; }

    bool IsInsideImplement() { return current_implement_type != nullptr; }

   public:
    std::shared_ptr<SymbolTable> symbol_table = nullptr;
    std::shared_ptr<ir::Module> module = nullptr;

    std::stack<std::shared_ptr<ir::Function>> function_stack;

    std::stack<std::shared_ptr<ir::Value>> loop_stack;

    std::stack<std::shared_ptr<ir::Block>> block_stack;

    ir::Block::iterator inserter_iterator;
    std::function<void(std::shared_ptr<ir::Value>)> create_callback;

    std::shared_ptr<Logger> logger = nullptr;

    std::stack<std::shared_ptr<ir::Type>> instantiating_type_stack;
    std::shared_ptr<ir::Type> current_implement_type = nullptr;
    std::shared_ptr<ir::InterfaceImplement> current_implement_interface = nullptr;
    boost::optional<std::list<Symbol>> symbol_template_argument_list_optional;
    bool is_static_function = false;
    bool interface_prototype_processing = false;

    int closure_id = 0;
};

}  // namespace prajna::lowering
