#pragma once

#include <algorithm>
#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>
#include <variant>

#include "prajna/ast/ast.hpp"
#include "prajna/helper.hpp"
#include "prajna/ir/ir.hpp"
#include "prajna/named.hpp"

namespace std {

inline bool operator==(std::shared_ptr<prajna::ir::ConstantInt> lhs,
                       std::shared_ptr<prajna::ir::ConstantInt> rhs) {
    return lhs->value == rhs->value;
}

}  // namespace std

namespace prajna::lowering {

class SymbolTable;
class TemplateStruct;
class Template;

// Symbol now inherits from std::variant
class Symbol
    : public std::variant<std::nullptr_t, std::shared_ptr<ir::Value>, std::shared_ptr<ir::Type>,
                          std::shared_ptr<TemplateStruct>, std::shared_ptr<Template>,
                          std::shared_ptr<ir::InterfacePrototype>, std::shared_ptr<SymbolTable>,
                          std::shared_ptr<ir::ConstantInt>> {
    using base = std::variant<std::nullptr_t, std::shared_ptr<ir::Value>, std::shared_ptr<ir::Type>,
                              std::shared_ptr<TemplateStruct>, std::shared_ptr<Template>,
                              std::shared_ptr<ir::InterfacePrototype>, std::shared_ptr<SymbolTable>,
                              std::shared_ptr<ir::ConstantInt>>;

   public:
    using base::variant;  // inherit constructors

    Symbol() : base(nullptr) {}

    std::string GetName() const;
    std::string GetFullname() const;
    void SetName(std::string name);
    void SetFullname(std::string fullname);
};

template <typename _T>
inline bool SymbolIs(const Symbol& symbol) {
    return std::holds_alternative<std::shared_ptr<_T>>(symbol);
}

template <typename _T>
inline auto SymbolGet(const Symbol& symbol) -> std::shared_ptr<_T> {
    if (auto ptr = std::get_if<std::shared_ptr<_T>>(&symbol)) return *ptr;
    return nullptr;
}

class SymbolTable : public std::enable_shared_from_this<SymbolTable> {
    SymbolTable() = default;

    SymbolTable(std::shared_ptr<SymbolTable> parent) : parent_symbol_table(parent) {}

   public:
    static std::shared_ptr<SymbolTable> Create(std::shared_ptr<SymbolTable> parent_symbol_table) {
        std::shared_ptr<SymbolTable> self(new SymbolTable(parent_symbol_table));
        return self;
    }

    void Set(const Symbol& value, const std::string& name) { current_symbol_dict[name] = value; }

    void SetWithAssigningName(Symbol value, const std::string& name);

    Symbol Get(const std::string& name) {
        if (current_symbol_dict.count(name) > 0) return current_symbol_dict[name];

        if (parent_symbol_table) {
            auto par_re = parent_symbol_table->Get(name);
            if (!std::holds_alternative<std::nullptr_t>(par_re)) return par_re;
        }

        return Symbol(nullptr);
    }

    bool CurrentTableHas(const std::string& name) {
        return current_symbol_dict.count(name) > 0 ? true : false;
    }

    Symbol CurrentTableGet(const std::string& name) { return this->current_symbol_dict[name]; }

    bool Has(const std::string& name) {
        auto symbol = this->Get(name);
        return !std::holds_alternative<std::nullptr_t>(symbol);
    }

    void Each(std::function<void(Symbol)> callback) {
        for (auto [key, symbol] : current_symbol_dict) {
            std::visit(overloaded{[callback](auto x) { callback(x); },
                                  [callback](std::shared_ptr<SymbolTable> symbol_table) {
                                      symbol_table->Each(callback);
                                  }},
                       symbol);
        }
    }

    void Finalize() {
        for (auto [key, symbol] : current_symbol_dict) {
            std::visit(overloaded{[](auto) {},
                                  [](std::shared_ptr<SymbolTable> symbol_table) {
                                      symbol_table->Finalize();
                                  }},
                       symbol);
        }

        parent_symbol_table = nullptr;
        current_symbol_dict.clear();
    }

    std::shared_ptr<SymbolTable> RootSymbolTable() {
        std::shared_ptr<SymbolTable> root_symbol_table = shared_from_this();
        while (root_symbol_table->parent_symbol_table) {
            root_symbol_table = root_symbol_table->parent_symbol_table;
        }

        return root_symbol_table;
    }

    std::string Fullname() {
        if (!parent_symbol_table) {
            return this->name;
        } else {
            if (!this->name.empty()) {
                return ConcatFullname(parent_symbol_table->Fullname(), this->name);
            } else {
                return parent_symbol_table->Fullname();
            }
        }
    }

   public:
    std::string name = "";
    std::filesystem::path directory_path;
    std::shared_ptr<SymbolTable> parent_symbol_table = nullptr;
    std::unordered_map<std::string, Symbol> current_symbol_dict;
};

}  // namespace prajna::lowering
