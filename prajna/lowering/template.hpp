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

    // using SpecialGenerator = std::function<Symbol(std::shared_ptr<ir::Module>)>;

    static std::shared_ptr<Template> create() {
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
                    symbolGet<ir::InterfacePrototype>(_instance_dict[symbol_template_arguments])) {
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
    static std::shared_ptr<TemplateStruct> create() {
        std::shared_ptr<TemplateStruct> self(new TemplateStruct);
        self->template_struct_impl = Template::create();
        return self;
    }

    std::shared_ptr<ir::StructType> instantiateStructAndImplement(
        std::list<Symbol> template_arguments, std::shared_ptr<ir::Module> ir_module) {
        if (struct_type_instance_dict.count(template_arguments) == 0) {
            struct_type_instance_dict[template_arguments];

            struct_type_instance_dict[template_arguments] =
                cast<ir::StructType>(symbolGet<ir::Type>(
                    template_struct_impl->instantiate(template_arguments, ir_module)));
            struct_type_instance_dict[template_arguments]->template_struct =
                this->shared_from_this();
            struct_type_instance_dict[template_arguments]->template_arguments = template_arguments;

            for (auto template_implement : template_implement_type_vec) {
                template_implement->instantiate(template_arguments, ir_module);
            }

            for (auto [ir_interface_prototype, template_implement_interface_for_type] :
                 template_implement_interface_for_type_dict) {
                template_implement_interface_for_type->instantiate(template_arguments, ir_module);
            }
        }

        return struct_type_instance_dict[template_arguments];
    }

    std::shared_ptr<Template> template_struct_impl = nullptr;
    std::vector<std::shared_ptr<Template>> template_implement_type_vec;
    std::unordered_map<Symbol, std::shared_ptr<Template>>
        template_implement_interface_for_type_dict;

    std::unordered_map<std::list<Symbol>, std::shared_ptr<ir::StructType>>
        struct_type_instance_dict;
};

}  // namespace prajna::lowering
