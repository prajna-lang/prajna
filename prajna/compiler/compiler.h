#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "prajna/compiler/stdio_callback.h"

namespace prajna {

class Logger;

namespace lowering {
class SymbolTable;
}

namespace ir {
class Module;
}

namespace jit {
class ExecutionEngine;
}

namespace jit {
class ExecutionEngine;
}

std::shared_ptr<lowering::SymbolTable> createPrimitiveTypes();

// @brief 负责将般若编译器的各个模块整合到一块, 以及和外界的交互
class Compiler : public std::enable_shared_from_this<Compiler> {
   public:
    struct Settings {
        bool print_result = true;
    };

   private:
    Compiler() = default;

   public:
    static std::shared_ptr<Compiler> create();

    void compileBuiltinSourceFiles(std::string builtin_sources_dir);

    std::shared_ptr<ir::Module> compileCode(std::string code,
                                            std::shared_ptr<lowering::SymbolTable> symbol_table,
                                            std::string file_name, bool is_interpreter);

    void bindBuiltinFunctions();

    void executeCodeInRelp(std::string command_line_code);

    size_t getSymbolValue(std::string symbol_name);

    void executateTestFunctions();

    void executeProgram(std::filesystem::path program_path);

    void executateMainFunction();

    void addPackageDirectoryPath(std::string package_directory);

    /**
     * @brief 编译单个源文件
     * @param[in] prajna_source_dir
     * @param[in]
     */
    std::shared_ptr<ir::Module> compileProgram(std::filesystem::path prajna_source_file,
                                               bool is_interpreter);

   public:
    std::shared_ptr<lowering::SymbolTable> _symbol_table;
    std::shared_ptr<jit::ExecutionEngine> jit_engine;
    std::vector<std::filesystem::path> package_directories;

    std::shared_ptr<Logger> logger = nullptr;

    Settings settings;
};

}  // namespace prajna
