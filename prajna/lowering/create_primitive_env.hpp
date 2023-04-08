#pragma once

#include "prajna/ir/ir.hpp"
#include "prajna/lowering/symbol_table.hpp"

namespace prajna::lowering {

inline std::shared_ptr<lowering::SymbolTable> createPrimitiveTypes() {
    auto new_symbol_table = lowering::SymbolTable::create(nullptr);

    auto add_primite_type = [=](std::shared_ptr<ir::Type> ir_type) {
        new_symbol_table->set(ir_type, ir_type->name);
    };

    add_primite_type(ir::BoolType::create());
    add_primite_type(ir::CharType::create());
    add_primite_type(ir::VoidType::create());
    add_primite_type(ir::UndefType::create());

    return new_symbol_table;
}

}  // namespace prajna::lowering
