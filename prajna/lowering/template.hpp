#pragma once

#include <unordered_map>

#include "prajna/ir/ir.hpp"
#include "prajna/lowering/symbol_table.hpp"
#include "prajna/named.hpp"

namespace std {

template <>
struct hash<std::list<prajna::lowering::Symbol>> {
    std::size_t operator()(std::list<prajna::lowering::Symbol> symbol_list) const noexcept {
        return symbol_list.size();
    }
};

}  // namespace std

namespace prajna::lowering {

template <typename _T>
class Template {
   protected:
    Template() = default;

   public:
    using Generator =
        std::function<std::shared_ptr<_T>(std::list<Symbol>, std::shared_ptr<ir::Module>)>;

    static std::shared_ptr<Template<_T>> create(Generator generator) {
        std::shared_ptr<Template<_T>> self(new Template<_T>);
        self->_struct_generator = generator;
        return self;
    };

    virtual std::shared_ptr<_T> getInstance(std::list<Symbol> template_arguments,
                                            std::shared_ptr<ir::Module> ir_module) {
        auto struct_type_instance = _instance_dict[template_arguments];
        if (struct_type_instance) {
            return struct_type_instance;
        } else {
            struct_type_instance = _struct_generator(template_arguments, ir_module);
            _instance_dict[template_arguments] = struct_type_instance;
            return struct_type_instance;
        }
    }

   protected:
    Generator _struct_generator;
    // list自己有operator==, 符合要求的
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
        // imlement_is_processing会确保 struct 实例化, implement实例化顺序执行, 而不产生递归
        if (implement_is_processing.count(template_arguments) == 0) {
            implement_is_processing[template_arguments] = false;
        }

        if (struct_type_instance_dict[template_arguments] == nullptr) {
            implement_is_processing[template_arguments] = true;
            // 执行下面的函数时, struct_type_instance_dict[template_arguments]会被赋值,
            // 以便解决嵌套的问题, 下面的赋值有冗余, 但还是再赋值一次
            struct_type_instance_dict[template_arguments] =
                template_struct_impl->getInstance(template_arguments, ir_module);
            implement_is_processing[template_arguments] = false;
        }

        if (!implement_is_processing[template_arguments]) {
            for (auto template_implement : template_implements) {
                if (template_implements_processed[template_arguments].count(template_implement) ==
                    0) {
                    template_implements_processed[template_arguments][template_implement] = false;
                }

                if (template_implements_processed[template_arguments][template_implement]) {
                    continue;
                } else {
                    implement_is_processing[template_arguments] = true;
                    template_implement->getInstance(template_arguments, ir_module);
                    template_implements_processed[template_arguments][template_implement] = true;
                    implement_is_processing[template_arguments] = false;
                }
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
