#include "prajna/lowering/statement_lowering_visitor.hpp"

#include <filesystem>
#include <fstream>

#include "prajna/compiler/compiler.h"

namespace prajna::lowering {

using std::filesystem::path;

Symbol StatementLoweringVisitor::operator()(ast::Import ast_import) {
    try {
        Symbol symbol;
        if (ast_import.identifier_path.is_root) {
            symbol = ir_builder->symbol_table->rootSymbolTable();
        } else {
            symbol = ir_builder->symbol_table;
        }
        PRAJNA_ASSERT(symbol.which() != 0);

        path directory_path;
        for (auto iter_ast_identifier = ast_import.identifier_path.identifiers.begin();
             iter_ast_identifier != ast_import.identifier_path.identifiers.end();
             ++iter_ast_identifier) {
            std::string current_identifer = iter_ast_identifier->identifier;

            if (symbol.type() != typeid(std::shared_ptr<SymbolTable>)) {
                break;
            }

            symbol = boost::apply_visitor(
                overloaded{
                    [=](auto x) -> Symbol {
                        PRAJNA_UNREACHABLE;
                        return nullptr;
                    },
                    [=](std::shared_ptr<SymbolTable> symbol_table) -> Symbol {
                        auto symbol = symbol_table->get(current_identifer);
                        if (symbol.which() != 0) {
                            return symbol;
                        } else {
                            auto new_symbol_table = lowering::SymbolTable::create(symbol_table);
                            symbol_table->set(new_symbol_table, current_identifer);
                            new_symbol_table->directory_path =
                                symbol_table->directory_path / path(current_identifer);
                            new_symbol_table->name = current_identifer;

                            path source_path;
                            // 存在xxx.prajna文件
                            if (std::filesystem::exists(symbol_table->directory_path /
                                                        path(current_identifer + ".prajna"))) {
                                source_path = symbol_table->directory_path /
                                              path(current_identifer + ".prajna");
                            } else {
                                // 存在xxx目录
                                if (std::filesystem::is_directory(symbol_table->directory_path /
                                                                  path(current_identifer))) {
                                    // xxx/.prajna存在
                                    if (std::filesystem::exists(symbol_table->directory_path /
                                                                path(current_identifer) /
                                                                ".prajna")) {
                                        source_path = symbol_table->directory_path /
                                                      path(current_identifer) / ".prajna";
                                    } else {  // xxx/.prajna不存在
                                        // 无需编译代码, 直接返回
                                        return new_symbol_table;
                                    }
                                }
                            }

                            std::ifstream ifs(source_path.string());
                            PRAJNA_ASSERT(ifs.good());
                            std::string code((std::istreambuf_iterator<char>(ifs)),
                                             std::istreambuf_iterator<char>());
                            this->compiler->compileCode(code, new_symbol_table,
                                                        source_path.string(), false);

                            return new_symbol_table;
                        }
                    }},
                symbol);

            PRAJNA_ASSERT(symbol.which() != 0);
        }

        // symbol = expression_lowering_visitor->applyIdentifierPath(ast_import.identifier_path);

        ir_builder->symbol_table->set(symbol,
                                      ast_import.identifier_path.identifiers.back().identifier);

        if (ast_import.as) {
            ir_builder->symbol_table->set(symbol, *ast_import.as);
        }

        return symbol;
    } catch (CompileError compile_error) {
        logger->note(ast_import.identifier_path);
        throw compile_error;
    }
}

}  // namespace prajna::lowering
