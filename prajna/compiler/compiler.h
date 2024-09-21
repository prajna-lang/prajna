#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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

extern std::function<void(std::string)> print_callback;
extern std::function<char*(void)> input_callback;

// @brief 负责将般若编译器的各个模块整合到一块, 以及和外界的交互
class Compiler : public std::enable_shared_from_this<Compiler> {
   public:
    struct Settings {
        bool print_result = true;
    };

   private:
    Compiler() = default;

   public:
    static std::shared_ptr<Compiler> Create(bool clean_types = true);

    void CompileBuiltinSourceFiles(std::string builtin_sources_dir);

    std::shared_ptr<ir::Module> CompileCode(std::string code,
                                            std::shared_ptr<lowering::SymbolTable> symbol_table,
                                            std::string file_name, bool is_interpreter);

    void GenLlvm(std::shared_ptr<ir::Module> ir_module);

    void BindBuiltinFunctions();

    void ExecuteCodeInRelp(std::string command_line_code);

    int64_t GetSymbolValue(std::string symbol_name);

    void RunTests(std::filesystem::path prajna_source_package_path);

    void ExecuteProgram(std::filesystem::path program_path);

    void ExecutateMainFunction();

    void AddPackageDirectoryPath(std::string package_directory);

    ~Compiler();

    /**
     * @brief 编译单个源文件
     * @param[in] prajna_source_dir
     * @param[in]
     */
    std::shared_ptr<ir::Module> CompileProgram(std::filesystem::path prajna_source_file,
                                               bool is_interpreter);

   public:
    std::shared_ptr<lowering::SymbolTable> _symbol_table;
    std::shared_ptr<jit::ExecutionEngine> jit_engine;
    std::vector<std::filesystem::path> package_directories;

    std::shared_ptr<Logger> logger = nullptr;

    Settings settings;
};

}  // namespace prajna
