#include "prajna/lowering/symbol_table.hpp"

#include "prajna/ir/ir.hpp"
#include "prajna/lowering/template.hpp"

namespace prajna::lowering {

std::string Symbol::GetName() const {
    return std::visit(
        overloaded{[](auto x) { return x->Name(); }, [](std::nullptr_t) { return std::string(); },
                   [](std::shared_ptr<ir::ConstantInt> ir_constant_int) {
                       return std::to_string(ir_constant_int->value);
                   },
                   [](std::shared_ptr<SymbolTable> symbol_table) { return symbol_table->name; }},
        *this);
}

std::string Symbol::GetFullname() const {
    return std::visit(
        overloaded{
            [](auto x) { return x->Fullname(); }, [](std::nullptr_t) { return std::string(); },
            [](std::shared_ptr<ir::ConstantInt> ir_constant_int) {
                return std::to_string(ir_constant_int->value);
            },
            [](std::shared_ptr<SymbolTable> symbol_table) { return symbol_table->Fullname(); }},
        *this);
}

void Symbol::SetName(std::string name) {
    std::visit(
        overloaded{[&](auto x) { x->Name(name); }, [&](std::nullptr_t) { PRAJNA_UNREACHABLE; },
                   [&](std::shared_ptr<ir::ConstantInt>) { PRAJNA_UNREACHABLE; },
                   [&](std::shared_ptr<SymbolTable> symbol_table) { symbol_table->name = name; }},
        *this);
}

void Symbol::SetFullname(std::string fullname) {
    std::visit(overloaded{[&](auto x) { x->Fullname(fullname); },
                          [&](std::nullptr_t) { PRAJNA_UNREACHABLE; },
                          [&](std::shared_ptr<ir::ConstantInt>) { PRAJNA_UNREACHABLE; },
                          [&](std::shared_ptr<SymbolTable> symbol_table) {
                              PRAJNA_UNREACHABLE;  // SymbolTable fullname is computed, not set
                          }},
               *this);
}

void SymbolTable::SetWithAssigningName(Symbol value, const std::string& name) {
    value.SetName(name);
    value.SetFullname(this->Fullname() + "::" + name);
    current_symbol_dict[name] = value;
}

}  // namespace prajna::lowering
