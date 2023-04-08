#include "prajna/compiler/compiler.h"

#include <filesystem>
#include <fstream>

#include "boost/algorithm/string.hpp"
#include "prajna/assert.hpp"
#include "prajna/codegen/llvm_codegen.h"
#include "prajna/exception.hpp"
#include "prajna/jit/execution_engine.h"
#include "prajna/logger.hpp"
#include "prajna/lowering/lower.h"
#include "prajna/parser/parse.h"
#include "prajna/transform/transform.h"

namespace prajna {

namespace {

inline std::shared_ptr<lowering::SymbolTable> createSymbolTableTree(
    std::shared_ptr<lowering::SymbolTable> root_symbol_table,
    std::string prajna_source_package_path) {
    auto source_file_path_string =
        std::filesystem::path(prajna_source_package_path).lexically_normal().string();
    std::vector<std::string> result;
    boost::split(result, source_file_path_string, boost::is_any_of("/"));
    result.back() = std::filesystem::path(result.back()).stem().string();
    auto symbol_table_tree = root_symbol_table;
    std::filesystem::path symbol_table_source_path;
    for (auto path_part : result) {
        // .prajna不增加名字空间,
        // https://en.cppreference.com/w/cpp/filesystem/path/stem, ".prajna"的stem仍然是".prajna"
        if (path_part == ".prajna") {
            continue;
        }

        symbol_table_source_path /= std::filesystem::path(path_part);
        if (symbol_table_tree->has(path_part)) {
            auto tmp_symbol_table_tree =
                lowering::symbolGet<lowering::SymbolTable>(symbol_table_tree->get(path_part));
            if (tmp_symbol_table_tree) {
                symbol_table_tree = tmp_symbol_table_tree;
                continue;
            }
        }

        auto new_symbol_table = lowering::SymbolTable::create(symbol_table_tree);
        symbol_table_tree->set(new_symbol_table, path_part);
        new_symbol_table->directory_path = symbol_table_source_path;
        new_symbol_table->name = path_part;
        symbol_table_tree = new_symbol_table;
    }

    return symbol_table_tree;
}

}  // namespace

std::shared_ptr<Compiler> Compiler::create() {
    ir::global_context = ir::GlobalContext(64);
    std::shared_ptr<Compiler> self(new Compiler);
    self->_symbol_table = lowering::SymbolTable::create(nullptr);
    self->jit_engine = std::make_shared<jit::ExecutionEngine>();
    self->jit_engine->bindBuiltinFunction();
    return self;
}

void Compiler::compileBuiltinSourceFiles(std::string builtin_sources_dir) {
    this->addPackageDirectoryPath(builtin_sources_dir);
    // this->compileProgram("prajna_bootstrap.prajna", false);
    this->compileProgram(".prajna", false);
}

std::shared_ptr<ir::Module> Compiler::compileCode(
    std::string code, std::shared_ptr<lowering::SymbolTable> symbol_table, std::string file_name,
    bool is_interpreter) {
    // interpreter模式时候, lowering会直接执行函数, 故这里开始就捕获异常
    jit_engine->catchRuntimeError();

    this->logger = Logger::create(code);
    auto ast = prajna::parser::parse(code, file_name, logger);
    PRAJNA_ASSERT(ast);
    auto ir_lowering_module =
        prajna::lowering::lower(ast, symbol_table, logger, shared_from_this(), is_interpreter);
    ir_lowering_module->name = file_name;
    ir_lowering_module->fullname = ir_lowering_module->name;
    for (auto [ir_target, ir_sub_module] : ir_lowering_module->modules) {
        if (ir_sub_module == nullptr) continue;
        ir_sub_module->name = ir_lowering_module->name + "_" + ir::targetToString(ir_target);
        ir_sub_module->fullname = ir_sub_module->name;
    }
    auto ir_ssa_module = prajna::transform::transform(ir_lowering_module);
    auto ir_codegen_module = prajna::codegen::llvmCodegen(ir_ssa_module, ir::Target::host);
    auto ir_llvm_optimize_module = prajna::codegen::llvmPass(ir_codegen_module);

    jit_engine->addIRModule(ir_codegen_module);

    return ir_lowering_module;
}

void Compiler::executeCodeInRelp(std::string script_code) {
    try {
        static int command_id = 0;

        auto ir_module = this->compileCode(script_code, _symbol_table,
                                           ":cmd" + std::to_string(command_id++), true);
        if (not ir_module) return;

        // @note 会有一次输入多个句子的情况
        for (auto ir_function : ir_module->functions) {
            if (ir_function->annotation_dict.count("\\command")) {
                auto fun_fullname = ir_function->fullname;
                auto fun_ptr = reinterpret_cast<void (*)(void)>(getSymbolValue(fun_fullname));
                fun_ptr();
            }
        }
    } catch (CompileError error) {
        ///  编译器需要继续执行其他指令
    }
}

void Compiler::executeProgram(std::filesystem::path program_path) {
    this->settings.print_result = false;
    bool is_script = program_path.extension() == ".prajnascript";
    auto ir_module = compileProgram(program_path, is_script);
    if (is_script) {
        for (auto ir_function : ir_module->functions) {
            if (ir_function->annotation_dict.count("\\command")) {
                auto fun_fullname = ir_function->fullname;
                auto fun_ptr = reinterpret_cast<void (*)(void)>(getSymbolValue(fun_fullname));
                fun_ptr();
            }
        }
    } else {
        executateMainFunction();
    }
}

void Compiler::executateTestFunctions() {
    std::set<std::shared_ptr<ir::Function>> function_tested_set;
    this->_symbol_table->each([=, &function_tested_set](lowering::Symbol symbol) {
        if (auto ir_value = lowering::symbolGet<ir::Value>(symbol)) {
            if (auto ir_function = cast<ir::Function>(ir_value)) {
                if (function_tested_set.count(ir_function)) {
                    return;
                }

                function_tested_set.insert(ir_function);
                if (ir_function->annotation_dict.count("test")) {
                    auto function_pointer = getSymbolValue(ir_function->fullname);
                    jit_engine->catchRuntimeError();
                    reinterpret_cast<void (*)(void)>(function_pointer)();
                }
            }
        }
    });
}

void Compiler::executateMainFunction() {
    std::set<std::shared_ptr<ir::Function>> main_functions;

    this->_symbol_table->each([=, &main_functions](lowering::Symbol symbol) {
        if (auto ir_value = lowering::symbolGet<ir::Value>(symbol)) {
            if (auto ir_function = cast<ir::Function>(ir_value)) {
                if (ir_function->name == "main") {
                    main_functions.insert(ir_function);
                    if (main_functions.size() >= 2) {
                        return;
                    }

                    auto function_pointer = getSymbolValue(ir_function->fullname);
                    jit_engine->catchRuntimeError();
                    reinterpret_cast<void (*)(void)>(function_pointer)();
                }
            }
        }
    });

    if (main_functions.empty()) {
        logger->error("the main function is not found");
    }

    if (main_functions.size() >= 2) {
        logger->error("the main function is duplicate");
        for (auto ir_function : main_functions) {
            logger->note(ir_function->source_location);
        }
    }
}

size_t Compiler::getSymbolValue(std::string symbol_name) {
    return this->jit_engine->getValue(symbol_name);
}

void Compiler::addPackageDirectoryPath(std::string package_directory) {
    if (!std::filesystem::is_directory(std::filesystem::path(package_directory))) {
        auto error_message = fmt::format("{} is not a valid package directory",
                                         fmt::styled(package_directory, fmt::fg(fmt::color::red)));
        fmt::print(error_message);
        throw std::runtime_error(error_message);
    }
    package_directories.push_back(std::filesystem::path(package_directory));
}

std::shared_ptr<ir::Module> Compiler::compileProgram(
    std::filesystem::path prajna_source_package_path, bool is_interpreter) {
    std::filesystem::path prajna_source_path;
    std::filesystem::path prajna_directory_path;
    for (auto package_directory : package_directories) {
        auto tmp_path =
            std::filesystem::current_path() / package_directory / prajna_source_package_path;
        if (std::filesystem::exists(tmp_path)) {
            prajna_directory_path = std::filesystem::current_path() / package_directory;
            prajna_source_path = tmp_path;
            break;
        }
    }

    if (prajna_source_path.empty()) {
        logger->error(
            fmt::format("{} is invalid program file\n", prajna_source_package_path.string()));
        throw CompileError();
        return nullptr;
    }

    auto current_symbol_table = createSymbolTableTree(_symbol_table, prajna_source_package_path);
    current_symbol_table->directory_path =
        prajna_directory_path / current_symbol_table->directory_path;

    std::ifstream ifs(prajna_source_path.string());
    if (ifs.good()) {
        std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        return this->compileCode(code, current_symbol_table, prajna_source_path.string(),
                                 is_interpreter);
    } else {
        logger->error("invalid program");
        return nullptr;
    }
}

}  // namespace prajna
