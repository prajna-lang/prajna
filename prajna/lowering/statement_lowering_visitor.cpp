#include "prajna/lowering/statement_lowering_visitor.hpp"

#include <filesystem>

#include "prajna/compiler/compiler.h"

namespace prajna::lowering {

Symbol StatementLoweringVisitor::operator()(ast::Import ast_import) {
    Symbol symbol;
    if (ast_import.identifier_path.is_root) {
        symbol = ir_builder->symbol_table->rootSymbolTable();
    } else {
        symbol = ir_builder->symbol_table;
    }
    PRAJNA_ASSERT(symbol.which() != 0);

    std::filesystem::path source_path;
    for (auto iter_ast_identifier = ast_import.identifier_path.identifiers.begin();
         iter_ast_identifier != ast_import.identifier_path.identifiers.end();
         ++iter_ast_identifier) {
        std::string current_identifer = iter_ast_identifier->identifier;

        if (symbol.type() != typeid(std::shared_ptr<SymbolTable>)) {
            break;
        }

        symbol = boost::apply_visitor(
            overloaded{[=](auto x) -> Symbol {
                           PRAJNA_UNREACHABLE;
                           return nullptr;
                       },
                       [=](std::shared_ptr<SymbolTable> symbol_table) -> Symbol {
                           auto symbol = symbol_table->get(current_identifer);
                           if (symbol.which() != 0) {
                               return symbol;
                           } else {
                               return compiler->compileFile(
                                   symbol_table->source_path /
                                   std::filesystem::path(current_identifer));
                           }
                       }},
            symbol);

        PRAJNA_ASSERT(symbol.which() != 0);
    }

    symbol = expression_lowering_visitor->applyIdentifiersResolution(ast_import.identifier_path);

    ir_builder->symbol_table->set(symbol, ast_import.identifier_path.identifiers.back().identifier);

    if (ast_import.as) {
        ir_builder->symbol_table->set(symbol, *ast_import.as);
    }

    return symbol;
}

}  // namespace prajna::lowering
