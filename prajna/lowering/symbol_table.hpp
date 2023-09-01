#pragma once

#include <algorithm>
#include <filesystem>
#include <memory>
#include <unordered_map>

#include "boost/variant.hpp"
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

using Symbol = boost::variant<std::nullptr_t, std::shared_ptr<ir::Value>, std::shared_ptr<ir::Type>,
                              std::shared_ptr<TemplateStruct>, std::shared_ptr<Template>,
                              std::shared_ptr<ir::InterfacePrototype>, std::shared_ptr<SymbolTable>,
                              std::shared_ptr<ir::ConstantInt>>;

template <typename _T>
inline bool SymbolIs(Symbol symbol) {
    return symbol.type() == typeid(std::shared_ptr<_T>);
}

template <typename _T>
inline auto SymbolGet(Symbol symbol) -> std::shared_ptr<_T> {
    if (SymbolIs<_T>(symbol)) {
        return boost::get<std::shared_ptr<_T>>(symbol);
    } else {
        return nullptr;
    }
}

class SymbolTable : public std::enable_shared_from_this<SymbolTable>, public Named {
    SymbolTable() = default;

    SymbolTable(std::shared_ptr<SymbolTable> parent) : parent_symbol_table(parent) {}

   public:
    static std::shared_ptr<SymbolTable> Create(std::shared_ptr<SymbolTable> parent_symbol_table) {
        std::shared_ptr<SymbolTable> self(new SymbolTable(parent_symbol_table));
        return self;
    }

    void Set(Symbol value, const std::string& name) { current_symbol_dict[name] = value; }

    void SetWithAssigningName(Symbol value, const std::string& name);

    Symbol Get(const std::string& name) {
        if (current_symbol_dict.count(name) > 0) return current_symbol_dict[name];

        if (parent_symbol_table) {
            auto par_re = parent_symbol_table->Get(name);
            if (par_re.which() != 0) return par_re;
        }

        return nullptr;
    }

    bool CurrentTableHas(const std::string& name) {
        return current_symbol_dict.count(name) > 0 ? true : false;
    }

    Symbol CurrentTableGet(const std::string& name) { return this->current_symbol_dict[name]; }

    bool Has(const std::string& name) {
        auto symbol = this->Get(name);
        return symbol.which() == 0 ? false : true;
    }

    void Each(std::function<void(Symbol)> callback) {
        for (auto [key, symbol] : current_symbol_dict) {
            boost::apply_visitor(overloaded{[callback](auto x) { callback(x); },
                                            [callback](std::shared_ptr<SymbolTable> symbol_table) {
                                                symbol_table->Each(callback);
                                            }},
                                 symbol);
        }
    }

    void Finalize() {
        for (auto [key, symbol] : current_symbol_dict) {
            boost::apply_visitor(overloaded{[](auto) {},
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

std::string SymbolGetName(Symbol symbol);

std::string SymbolGetFullname(Symbol symbol);

void SymbolSetName(std::string name, Symbol symbol);

void SymbolSetFullname(std::string fullname, Symbol symbol);

ast::SourceLocation SymbolGetSourceLocation(Symbol symbol);

}  // namespace prajna::lowering
