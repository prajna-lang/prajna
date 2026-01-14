#include "prajna/lowering/symbol_table.hpp"

#include "prajna/ir/ir.hpp"
#include "prajna/lowering/template.hpp"

namespace prajna::lowering {

std::string Symbol::GetName() const {
    return std::visit(
        overloaded{[](auto x) { return x->name; }, [](std::nullptr_t) { return std::string(); },
                   [](std::shared_ptr<ir::ConstantInt> ir_constant_int) {
                       return std::to_string(ir_constant_int->value);
                   }},
        *this);
}

std::string Symbol::GetFullname() const {
    return std::visit(
        overloaded{
            [](auto x) { return x->fullname; }, [](std::nullptr_t) { return std::string(); },
            [](std::shared_ptr<ir::ConstantInt> ir_constant_int) {
                return std::to_string(ir_constant_int->value);
            },
            [](std::shared_ptr<SymbolTable> symbol_table) { return symbol_table->Fullname(); }},
        *this);
}

void Symbol::SetName(std::string name) {
    std::visit(
        overloaded{[&](auto x) { x->name = name; }, [&](std::nullptr_t) { PRAJNA_UNREACHABLE; },
                   [&](std::shared_ptr<ir::ConstantInt>) { PRAJNA_UNREACHABLE; }},
        *this);
}

void Symbol::SetFullname(std::string fullname) {
    std::visit(overloaded{[&](auto x) { x->fullname = fullname; },
                          [&](std::nullptr_t) { PRAJNA_UNREACHABLE; },
                          [&](std::shared_ptr<ir::ConstantInt>) { PRAJNA_UNREACHABLE; },
                          [&](std::shared_ptr<SymbolTable>) { PRAJNA_UNREACHABLE; }},
               *this);
}

void SymbolTable::SetWithAssigningName(Symbol value, const std::string& name) {
    value.SetName(name);
    value.SetFullname(this->Fullname() + "::" + name);
    current_symbol_dict[name] = value;
}

}  // namespace prajna::lowering
