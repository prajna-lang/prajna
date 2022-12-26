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
    add_primite_type(ir::IntType::create(8, true));
    add_primite_type(ir::IntType::create(16, true));
    add_primite_type(ir::IntType::create(32, true));
    add_primite_type(ir::IntType::create(64, true));
    add_primite_type(ir::IntType::create(8, false));
    add_primite_type(ir::IntType::create(16, false));
    add_primite_type(ir::IntType::create(32, false));
    add_primite_type(ir::IntType::create(64, false));
    add_primite_type(ir::FloatType::create(32));
    add_primite_type(ir::FloatType::create(64));

    return new_symbol_table;
}

}  // namespace prajna::lowering
