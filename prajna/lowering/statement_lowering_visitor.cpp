#include "prajna/lowering/statement_lowering_visitor.hpp"

#include <filesystem>
#include <fstream>
#include <future>
#include <optional>

#include "boost/asio/io_service.hpp"
#include "boost/process/io.hpp"
#include "boost/process/search_path.hpp"
#include "boost/process/system.hpp"
#include "prajna/compiler/compiler.h"
#include "prajna/jit/execution_engine.h"

namespace prajna::lowering {

using std::filesystem::path;

std::optional<path> GetPackagePath(path directory_path, std::string identifier) {
    if (std::filesystem::exists(directory_path / path(identifier + ".prajna"))) {
        return directory_path / path(identifier + ".prajna");
    } else {
        // 存在xxx目录
        if (std::filesystem::is_directory(directory_path / path(identifier))) {
            // xxx/.prajna存在
            if (std::filesystem::exists(directory_path / path(identifier) / ".prajna")) {
                return directory_path / path(identifier) / ".prajna";
            } else {  // xxx/.prajna不存在
                // 无需编译代码, 直接返回
                return directory_path / path(identifier);
            }
        } else {
            return std::nullopt;
        }
    }
}

Symbol StatementLoweringVisitor::operator()(ast::Use ast_import) {
    try {
        Symbol symbol;
        if (ast_import.identifier_path.root_optional) {
            symbol = ir_builder->symbol_table->RootSymbolTable();
        } else {
            symbol = ir_builder->symbol_table;
        }
        PRAJNA_ASSERT(symbol.which() != 0);

        bool is_first_module = true;

        path directory_path;
        for (auto iter_ast_identifier = ast_import.identifier_path.identifiers.begin();
             iter_ast_identifier != ast_import.identifier_path.identifiers.end();
             ++iter_ast_identifier) {
            std::string identifier = iter_ast_identifier->identifier;
            if (symbol.type() != typeid(std::shared_ptr<SymbolTable>)) {
                break;
            }

            symbol = boost::apply_visitor(
                overloaded{
                    [=](auto x) -> Symbol {
                        PRAJNA_UNREACHABLE;
                        return nullptr;
                    },
                    [=, &is_first_module](std::shared_ptr<SymbolTable> symbol_table) -> Symbol {
                        auto symbol = symbol_table->Get(identifier);
                        if (symbol.which() != 0) {
                            return symbol;
                        } else {
                            auto new_symbol_table = lowering::SymbolTable::Create(symbol_table);
                            symbol_table->Set(new_symbol_table, identifier);
                            new_symbol_table->directory_path =
                                symbol_table->directory_path / path(identifier);
                            new_symbol_table->name = identifier;

                            auto package_path_optional =
                                GetPackagePath(symbol_table->directory_path, identifier);

                            if (package_path_optional) {
                                auto new_symbol_table = lowering::SymbolTable::Create(symbol_table);
                                symbol_table->Set(new_symbol_table, identifier);
                                new_symbol_table->directory_path =
                                    symbol_table->directory_path / path(identifier);
                                new_symbol_table->name = identifier;
                                auto package_path = *package_path_optional;
                                if (!std::filesystem::is_directory(package_path)) {
                                    std::ifstream ifs(package_path.string());
                                    PRAJNA_ASSERT(ifs.good());
                                    std::string code((std::istreambuf_iterator<char>(ifs)),
                                                     std::istreambuf_iterator<char>());
                                    this->compiler->CompileCode(code, new_symbol_table,
                                                                package_path.string(), false);
                                }
                                return new_symbol_table;
                            } else {
                                // 可以在搜索路径搜索
                                if (is_first_module) {
                                    for (auto package_directory : compiler->package_directories) {
                                        auto global_package_path_option =
                                            GetPackagePath(package_directory, identifier);
                                        if (global_package_path_option) {
                                            auto new_symbol_table = lowering::SymbolTable::Create(
                                                compiler->_symbol_table);
                                            compiler->_symbol_table->Set(new_symbol_table,
                                                                         identifier);
                                            symbol_table->Set(new_symbol_table, identifier);
                                            new_symbol_table->directory_path =
                                                package_directory / path(identifier);
                                            new_symbol_table->name = identifier;
                                            auto package_path = *global_package_path_option;
                                            if (!std::filesystem::is_directory(package_path)) {
                                                std::ifstream ifs(package_path.string());
                                                PRAJNA_ASSERT(ifs.good());
                                                std::string code(
                                                    (std::istreambuf_iterator<char>(ifs)),
                                                    std::istreambuf_iterator<char>());
                                                this->compiler->CompileCode(code, new_symbol_table,
                                                                            package_path.string(),
                                                                            false);
                                            }
                                            return new_symbol_table;
                                        }
                                    }
                                }

                                logger->Error("module is not found",
                                              iter_ast_identifier->identifier);
                                return nullptr;
                            }
                        }
                    }},
                symbol);

            is_first_module = false;
            PRAJNA_ASSERT(symbol.which() != 0);
        }

        ir_builder->symbol_table->Set(symbol,
                                      ast_import.identifier_path.identifiers.back().identifier);

        if (ast_import.identifier_path.identifiers.back().template_arguments_optional) {
            auto symbol_template_arguments =
                this->expression_lowering_visitor->ApplyTemplateArguments(
                    ast_import.identifier_path.identifiers.back()
                        .template_arguments_optional.get());
            if (auto template_struct = SymbolGet<TemplateStruct>(symbol)) {
                // 如果获取到nullptr则说明实例化正在进行中,
                // 使用instantiating_type来获取相应类型
                if (auto ir_type = template_struct->Instantiate(symbol_template_arguments,
                                                                ir_builder->module)) {
                    symbol = ir_type;
                } else {
                    PRAJNA_ASSERT(ir_builder->instantiating_type_stack.size());
                    symbol = ir_builder->instantiating_type_stack.top();
                }
            }
            if (auto tempate_ = SymbolGet<Template>(symbol)) {
                symbol = tempate_->instantiate(symbol_template_arguments, ir_builder->module);
            }
        }

        if (ast_import.as_optional) {
            ir_builder->symbol_table->Set(symbol, ast_import.as_optional.get());
        }

        return symbol;
    } catch (CompileError compile_error) {
        logger->Note(ast_import.identifier_path);
        throw compile_error;
    }
}

Symbol StatementLoweringVisitor::operator()(ast::Pragma ast_pragma) {
    // 编译时输出消息
    if (ast_pragma.name == "error") {
        std::string msg = ast_pragma.values.size() ? ast_pragma.values.front().value : "";
        logger->Error(fmt::format("pragma error: {}", msg), ast_pragma);
        return nullptr;
    }
    if (ast_pragma.name == "warning") {
        std::string msg = ast_pragma.values.size() ? ast_pragma.values.front().value : "";
        logger->Warning(fmt::format("pragma warning: {}", msg), ast_pragma);
        return nullptr;
    }

    // 执行系统指令
    if (ast_pragma.name == "system") {
        std::string command = ast_pragma.values.size() ? ast_pragma.values.front().value : "";

        boost::asio::io_service ios;
        std::future<std::string> future_stdout;
        boost::process::system(command, boost::process::std_out > future_stdout);
        print_callback(future_stdout.get().c_str());
        return nullptr;
    }

    // 系统预加载
    if (ast_pragma.name == "stage0") {
        this->Stage0();
        return nullptr;
    }
    if (ast_pragma.name == "stage1") {
        this->Stage1();
        return nullptr;
    }

    // 链接
    if (ast_pragma.name == "link") {
        if (ast_pragma.values.size() != 1) {
            logger->Error("#link should have one value");
        }
        auto dynamic_lib_name = ast_pragma.values.front().value;
        this->compiler->jit_engine->LoadDynamicLib(dynamic_lib_name);
        return nullptr;
    }

    logger->Error("the pragma is undefined", ast_pragma);
    return nullptr;
}

}  // namespace prajna::lowering
