#pragma once

#include <memory>

#include "llvm/IR/Module.h"
#include "prajna/ir/ir.hpp"

namespace prajna::ir {
class Module;
}

namespace prajna::codegen {

std::shared_ptr<ir::Module> LlvmCodegen(std::shared_ptr<ir::Module> ir_modul);

std::shared_ptr<ir::Module> LlvmPass(std::shared_ptr<ir::Module> ir_module);

}  // namespace prajna::codegen
