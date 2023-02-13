#pragma once

#include <unordered_map>

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

}  // namespace std

namespace prajna::lowering {

template <typename _T>
class Template : public Named {
   protected:
    Template() = default;

   public:
    using Generator =
        std::function<std::shared_ptr<_T>(std::list<Symbol>, std::shared_ptr<ir::Module>)>;

    static std::shared_ptr<Template<_T>> create(Generator generator) {
        std::shared_ptr<Template<_T>> self(new Template<_T>);
        self->_generator = generator;
        return self;
    };

    virtual std::shared_ptr<_T> getInstance(std::list<Symbol> symbol_template_arguments,
                                            std::shared_ptr<ir::Module> ir_module) {
        if (!_instance_dict.count(symbol_template_arguments)) {
            _instance_dict[symbol_template_arguments];  // 插入默认值, 阻断多次实力化
            _instance_dict[symbol_template_arguments] =
                _generator(symbol_template_arguments, ir_module);
        }
        return _instance_dict[symbol_template_arguments];
    }

   protected:
    Generator _generator;

    // list自己有operator==
    std::unordered_map<std::list<Symbol>, std::shared_ptr<_T>> _instance_dict;
};

struct ImplementTag {};

class TemplateStruct : public Named {
   public:
    TemplateStruct() = default;

   public:
    typedef Template<ir::StructType> Impl;
    typedef Template<ImplementTag> Implement;

    static std::shared_ptr<TemplateStruct> create(Impl::Generator struct_generator) {
        std::shared_ptr<TemplateStruct> self(new TemplateStruct);
        self->template_struct_impl = Impl::create(struct_generator);
        return self;
    }

    std::shared_ptr<ir::StructType> getStructInstance(std::list<Symbol> template_arguments,
                                                      std::shared_ptr<ir::Module> ir_module) {
        if (struct_type_instance_dict.count(template_arguments) == 0) {
            struct_type_instance_dict[template_arguments];

            struct_type_instance_dict[template_arguments] =
                template_struct_impl->getInstance(template_arguments, ir_module);
            struct_type_instance_dict[template_arguments]->template_arguments = template_arguments;

            for (auto template_implement : template_implements) {
                template_implement->getInstance(template_arguments, ir_module);
            }
        }

        return struct_type_instance_dict[template_arguments];
    }

    void pushBackImplements(std::shared_ptr<Implement> implement) {
        this->template_implements.push_back(implement);
    }

    bool is_processing = false;
    std::shared_ptr<Impl> template_struct_impl = nullptr;
    std::vector<std::shared_ptr<Implement>> template_implements;
    std::unordered_map<std::list<Symbol>, bool> implement_is_processing;
    std::unordered_map<std::list<Symbol>, std::unordered_map<std::shared_ptr<Implement>, bool>>
        template_implements_processed;
    std::unordered_map<std::list<Symbol>, std::shared_ptr<ir::StructType>>
        struct_type_instance_dict;
    std::list<std::string> template_parameter_identifier_list;
};

}  // namespace prajna::lowering
