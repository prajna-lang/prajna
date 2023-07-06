#pragma once

#include <unordered_map>

#include "boost/range/combine.hpp"
#include "prajna/ir/ir.hpp"
#include "prajna/lowering/symbol_table.hpp"
#include "prajna/named.hpp"

namespace std {

template <>
struct hash<std::list<prajna::lowering::Symbol>> {
    std::size_t operator()(
        std::list<prajna::lowering::Symbol> symbol_template_parameter_list) const noexcept {
        return symbol_template_parameter_list.size();
    }
};

template <>
struct hash<prajna::lowering::Symbol> {
    std::size_t operator()(prajna::lowering::Symbol symbol) const noexcept { return 1; }
};

}  // namespace std

namespace prajna::lowering {

class Template : public Named, public std::enable_shared_from_this<Template> {
   protected:
    Template() = default;

   public:
    using Generator = std::function<Symbol(std::list<Symbol>, std::shared_ptr<ir::Module>)>;

    static std::shared_ptr<Template> Create() {
        std::shared_ptr<Template> self(new Template);
        return self;
    };

    static int conceptCompatibility(Symbol template_argument, Symbol template_concept) {
        if (template_concept.which() == 0) {
            return 1;
        }

        if (template_argument == template_concept) {
            return 10;
        }

        return 0;
    }

    static int conceptCompatibility(std::list<Symbol> template_arguments,
                                    std::list<Symbol> template_concepts) {
        PRAJNA_ASSERT(template_arguments.size() == template_concepts.size());
        int compatibility = 0;
        for (auto [template_argument, template_concept] :
             boost::combine(template_arguments, template_concepts)) {
            auto cur_compatibility = conceptCompatibility(template_argument, template_concept);
            if (cur_compatibility <= 0) {
                return 0;
            } else {
                compatibility += cur_compatibility;
            }
        }

        return compatibility;
    }

    Symbol instantiate(std::list<Symbol> symbol_template_arguments,
                       std::shared_ptr<ir::Module> ir_module) {
        if (!_instance_dict.count(symbol_template_arguments)) {
            _instance_dict[symbol_template_arguments];  // 插入默认值, 阻断多次实力化,

            _instance_dict[symbol_template_arguments] =
                this->generator(symbol_template_arguments, ir_module);

            if (auto ir_interface_prototype =
                    SymbolGet<ir::InterfacePrototype>(_instance_dict[symbol_template_arguments])) {
                ir_interface_prototype->template_interface = this->shared_from_this();
                ir_interface_prototype->template_arguments = symbol_template_arguments;
            }
        }
        return _instance_dict[symbol_template_arguments];
    }

   private:
    std::unordered_map<std::list<Symbol>, Symbol> _instance_dict;

   public:
    Generator generator;
    size_t template_parameters_size = 0;
};

// class TemplateCollection : public Named, public std::enable_shared_from_this<TemplateCollection>
// {
//    public:

// };

class TemplateStruct : public Named, public std::enable_shared_from_this<TemplateStruct> {
   protected:
    TemplateStruct() = default;

   public:
    static std::shared_ptr<TemplateStruct> Create() {
        std::shared_ptr<TemplateStruct> self(new TemplateStruct);
        self->template_struct_impl = Template::Create();
        return self;
    }

    std::shared_ptr<ir::Type> Instantiate(std::list<Symbol> template_arguments,
                                          std::shared_ptr<ir::Module> ir_module,
                                          bool inside_struct = false) {
        // imlement_is_processing会确保 struct 实例化, implement实例化顺序执行, 而不产生递归
        if (implement_is_processing.count(template_arguments) == 0) {
            implement_is_processing[template_arguments] = false;
        }

        if (!implement_is_processing[template_arguments]) {
            implement_is_processing[template_arguments] = true;
            struct_type_instance_dict[template_arguments] = SymbolGet<ir::Type>(
                template_struct_impl->instantiate(template_arguments, ir_module));
            PRAJNA_ASSERT(struct_type_instance_dict[template_arguments]);
            struct_type_instance_dict[template_arguments]->template_struct =
                this->shared_from_this();
            struct_type_instance_dict[template_arguments]->template_arguments_any =
                template_arguments;
            implement_is_processing[template_arguments] = false;
        }

        if (!implement_is_processing[template_arguments] && !inside_struct) {
            for (auto template_implement : template_implement_type_vec) {
                implement_is_processing[template_arguments] = true;
                template_implement->instantiate(template_arguments, ir_module);
                implement_is_processing[template_arguments] = false;
            }
        }

        // 当返回nullptr时, 会使用ir_builder->instantiating_type_stack.top()去获取类型
        return struct_type_instance_dict[template_arguments];
    }

    std::shared_ptr<Template> template_struct_impl = nullptr;
    std::vector<std::shared_ptr<Template>> template_implement_type_vec;

    std::unordered_map<std::list<Symbol>, bool> implement_is_processing;

    std::unordered_map<std::list<Symbol>, std::shared_ptr<ir::Type>> struct_type_instance_dict;
};

inline std::string GetTemplateArgumentsPostify(std::list<Symbol> symbol_list) {
    std::string re = "<";
    for (auto iter = symbol_list.begin(); iter != symbol_list.end(); ++iter) {
        re.append(SymbolGetFullname(*iter));
        if (std::next(iter) == symbol_list.end()) {
            break;
        }
        re.append(", ");
    }
    re.append(">");
    return re;
}

}  // namespace prajna::lowering
