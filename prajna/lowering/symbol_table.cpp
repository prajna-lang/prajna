#include "prajna/lowering/symbol_table.hpp"

#include "boost/variant/variant.hpp"
#include "prajna/ir/ir.hpp"
#include "prajna/lowering/template.hpp"

namespace prajna::lowering {

std::string symbolGetName(Symbol symbol) {
    return boost::apply_visitor(
        overloaded{[](auto x) { return x->name; }, [](std::nullptr_t) { return std::string(); },
                   [](std::shared_ptr<ir::ConstantInt> ir_constant_int) {
                       return std::to_string(ir_constant_int->value);
                   }},
        symbol);
}

std::string symbolGetFullname(Symbol symbol) {
    return boost::apply_visitor(
        overloaded{
            [](auto x) { return x->fullname; }, [](std::nullptr_t) { return std::string(); },
            [](std::shared_ptr<ir::ConstantInt> ir_constant_int) {
                return std::to_string(ir_constant_int->value);
            },
            [](std::shared_ptr<SymbolTable> symbol_table) { return symbol_table->fullname(); }},
        symbol);
}

void symbolSetName(std::string name, Symbol symbol) {
    boost::apply_visitor(
        overloaded{[](std::string name, auto x) { x->name = name; },
                   [](std::string, std::nullptr_t) { PRAJNA_UNREACHABLE; },
                   [](std::string name, std::shared_ptr<ir::ConstantInt> ir_constant_int) {
                       PRAJNA_UNREACHABLE;
                   }},
        boost::variant<std::string>(name), symbol);
}

void symbolSetFullname(std::string fullname, Symbol symbol) {
    boost::apply_visitor(
        overloaded{
            [](std::string fullname, auto x) { x->fullname = fullname; },
            [](std::string, std::nullptr_t) { PRAJNA_UNREACHABLE; },
            [](std::string, std::shared_ptr<ir::ConstantInt> ir_constant_int) {
                PRAJNA_UNREACHABLE;
            },
            [](std::string, std::shared_ptr<SymbolTable> symbol_table) { PRAJNA_UNREACHABLE; }},
        boost::variant<std::string>(fullname), symbol);
}

void SymbolTable::setWithName(Symbol value, const std::string& name) {
    symbolSetName(name, value);
    symbolSetFullname(this->fullname() + "::" + name, value);
    current_symbol_dict[name] = value;
}

}  // namespace prajna::lowering
