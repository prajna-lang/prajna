#include "prajna/compiler/compiler.h"

#include <filesystem>
#include <fstream>

#include "boost/algorithm/string.hpp"
#include "prajna/assert.hpp"
#include "prajna/codegen/llvm_codegen.h"
#include "prajna/exception.hpp"
#include "prajna/jit/execution_engine.h"
#include "prajna/logger.hpp"
#include "prajna/lowering/create_primitive_env.hpp"
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
        symbol_table_source_path /= std::filesystem::path(path_part);
        if (symbol_table_tree->has(path_part)) {
            symbol_table_tree =
                lowering::symbolGet<lowering::SymbolTable>(symbol_table_tree->get(path_part));
        } else {
            auto new_symbol_table = lowering::SymbolTable::create(symbol_table_tree);
            symbol_table_tree->set(new_symbol_table, path_part);
            new_symbol_table->source_path = symbol_table_source_path;
            new_symbol_table->name = path_part;
            symbol_table_tree = new_symbol_table;
        }
    }

    return symbol_table_tree;
}

}  // namespace

std::shared_ptr<Compiler> Compiler::create() {
    ir::global_context = ir::GlobalContext(64);
    std::shared_ptr<Compiler> self(new Compiler);
    self->_symbol_table = lowering::createPrimitiveTypes();
    self->jit_engine = std::make_shared<jit::ExecutionEngine>();
    self->jit_engine->bindBuiltinFunction();
    return self;
}

void Compiler::compileBuiltinSourceFiles(std::string builtin_sources_dir) {
    this->addPackageDirectories(builtin_sources_dir);
    this->compileFile("bootstrap.prajna");
}

std::shared_ptr<ir::Module> Compiler::compileCode(
    std::string code, std::shared_ptr<lowering::SymbolTable> symbol_table, std::string file_name,
    bool is_interpreter) {
    try {
        auto logger = Logger::create(code);
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
        // auto ir_llvm_optimize_module = prajna::codegen::llvmPass(ir_codegen_module);

        jit_engine->addIRModule(ir_codegen_module);

        return ir_lowering_module;
    } catch (CompileError &) {
        ++compile_error_count;
        return nullptr;
    }
}

void Compiler::compileCommandLine(std::string command_line_code) {
    static int command_id = 0;
    auto ir_module = this->compileCode(command_line_code, _symbol_table,
                                       ":cmd" + std::to_string(command_id++), true);
    if (not ir_module) return;

    jit_engine->catchRuntimeError();
    // @note 会有一次输入多个句子的情况
    for (auto ir_function : ir_module->functions) {
        if (ir_function->annotations.count("\\command")) {
            auto fun_fullname = ir_function->fullname;
            auto fun_ptr = reinterpret_cast<void (*)(void)>(jit_engine->getValue(fun_fullname));
            fun_ptr();
        }
    }
}

void Compiler::runTestFunctions() {
    std::set<std::shared_ptr<ir::Function>> function_tested_set;
    this->_symbol_table->each(
        [jit_engine = this->jit_engine, &function_tested_set](lowering::Symbol symbol) {
            if (auto ir_value = lowering::symbolGet<ir::Value>(symbol)) {
                if (auto ir_function = cast<ir::Function>(ir_value)) {
                    if (function_tested_set.count(ir_function)) {
                        return;
                    }

                    function_tested_set.insert(ir_function);
                    if (ir_function->annotations.count("test")) {
                        auto function_pointer = jit_engine->getValue(ir_function->fullname);
                        jit_engine->catchRuntimeError();
                        reinterpret_cast<void (*)(void)>(function_pointer)();
                    }
                }
            }
        });
}

size_t Compiler::getSymbolValue(std::string symbol_name) {
    return this->jit_engine->getValue(symbol_name);
}

void Compiler::addPackageDirectories(std::string package_directory) {
    PRAJNA_ASSERT(std::filesystem::is_directory(std::filesystem::path(package_directory)));
    package_directories.push_back(std::filesystem::path(package_directory));
}

std::shared_ptr<lowering::SymbolTable> Compiler::compileFile(
    std::filesystem::path prajna_source_package_path) {
    std::filesystem::path prajna_source_path;
    for (auto package_directory : package_directories) {
        auto tmp_path =
            std::filesystem::current_path() / package_directory / prajna_source_package_path;
        if (std::filesystem::exists(tmp_path)) {
            prajna_source_path = tmp_path;
            break;
        }
    }

    if (prajna_source_path.empty()) {
        compile_error_count += 1;
        return nullptr;
    }

    // PRAJNA_ASSERT(!prajna_source_path.empty());

    auto current_symbol_table = createSymbolTableTree(_symbol_table, prajna_source_package_path);

    std::ifstream ifs(prajna_source_path.string());
    if (ifs.good()) {
        std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        this->compileCode(code, current_symbol_table, prajna_source_path.string(), false);
    }

    return current_symbol_table;
}

}  // namespace prajna
