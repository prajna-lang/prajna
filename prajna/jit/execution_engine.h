
#pragma once

#include <string>
#include <memory>

namespace prajna::ir {
class Module;
}

namespace llvm::orc {
class LLJIT;
}

namespace prajna::jit {

class ExecutionEngine {
   public:
    ExecutionEngine();

    int64_t GetValue(std::string name);

    void AddIRModule(std::shared_ptr<ir::Module> ir_module);

    bool LoadDynamicLib(std::string lib_name);

    void BindCFunction(void* fun_ptr, std::string mangle_name);

    void CatchRuntimeError();

    void BindBuiltinFunction();

   private:
    std::shared_ptr<llvm::orc::LLJIT> _up_lljit;
};

}  // namespace prajna::jit
