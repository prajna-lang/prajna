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

namespace fs = std::filesystem;

using Symbol = boost::variant<std::nullptr_t, std::shared_ptr<ir::Value>, std::shared_ptr<ir::Type>,
                              std::shared_ptr<TemplateStruct>, std::shared_ptr<SymbolTable>,
                              std::shared_ptr<ir::ConstantInt>>;

template <typename _T>
inline bool symbolIs(Symbol symbol) {
    return symbol.type() == typeid(std::shared_ptr<_T>);
}

template <typename _T>
inline auto symbolGet(Symbol symbol) -> std::shared_ptr<_T> {
    if (symbolIs<_T>(symbol)) {
        return boost::get<std::shared_ptr<_T>>(symbol);
    } else {
        return nullptr;
    }
}

class SymbolTable : public std::enable_shared_from_this<SymbolTable>, public Named {
    SymbolTable() = default;

    SymbolTable(std::shared_ptr<SymbolTable> parent) : parent_symbol_table(parent) {}

   public:
    static std::shared_ptr<SymbolTable> create(std::shared_ptr<SymbolTable> parent_symbol_table) {
        std::shared_ptr<SymbolTable> self(new SymbolTable(parent_symbol_table));
        return self;
    }

    void set(Symbol value, const std::string& name) { current_symbol_dict[name] = value; }

    void setWithName(Symbol value, const std::string& name);

    Symbol get(const std::string& name) {
        if (current_symbol_dict.count(name) > 0) return current_symbol_dict[name];

        if (parent_symbol_table) {
            auto par_re = parent_symbol_table->get(name);
            if (par_re.which() != 0) return par_re;
        }

        return nullptr;
    }

    bool currentTableHas(const std::string& name) {
        return current_symbol_dict.count(name) > 0 ? true : false;
    }

    bool has(const std::string& name) {
        auto symbol = this->get(name);
        return symbol.which() == 0 ? false : true;
    }

    void each(std::function<void(Symbol)> callback) {
        for (auto [key, symbol] : current_symbol_dict) {
            boost::apply_visitor(overloaded{[callback](auto x) { callback(x); },
                                            [callback](std::shared_ptr<SymbolTable> symbol_table) {
                                                symbol_table->each(callback);
                                            }},
                                 symbol);
        }
    }

    std::shared_ptr<SymbolTable> rootSymbolTable() {
        std::shared_ptr<SymbolTable> root_symbol_table = shared_from_this();
        while (root_symbol_table->parent_symbol_table) {
            root_symbol_table = root_symbol_table->parent_symbol_table;
        }

        return root_symbol_table;
    }

    std::string fullname() {
        if (!parent_symbol_table) {
            return this->name;
        } else {
            if (!this->name.empty()) {
                return concatFullname(parent_symbol_table->fullname(), this->name);
            } else {
                return parent_symbol_table->fullname();
            }
        }
    }

   public:
    std::string name = "";
    std::shared_ptr<SymbolTable> parent_symbol_table = nullptr;
    std::unordered_map<std::string, Symbol> current_symbol_dict;
};

inline std::shared_ptr<SymbolTable> createSymbolTableTree(
    std::shared_ptr<SymbolTable> root_symbol_table, std::string prajna_source_file) {
    fs::path source_file_path = fs::path(prajna_source_file);
    auto symbol_table_tree = root_symbol_table;
    for (auto path_part : source_file_path.stem()) {
        if (symbol_table_tree->has(path_part)) {
            symbol_table_tree =
                lowering::symbolGet<lowering::SymbolTable>(symbol_table_tree->get(path_part));
        } else {
            auto new_symbol_table = lowering::SymbolTable::create(symbol_table_tree);
            symbol_table_tree->set(new_symbol_table, path_part);
            new_symbol_table->name = path_part;
            symbol_table_tree = new_symbol_table;
        }
    }

    return symbol_table_tree;
}

std::string symbolGetName(Symbol symbol);

std::string symbolGetFullname(Symbol symbol);

void symbolSetName(std::string name, Symbol symbol);

void symbolSetFullname(std::string fullname, Symbol symbol);

}  // namespace prajna::lowering
