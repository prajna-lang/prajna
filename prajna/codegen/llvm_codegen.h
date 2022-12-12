#pragma once

#include <memory>

#include "llvm/IR/Module.h"
#include "prajna/ir/ir.hpp"

namespace llvm {
class Module;
class LLVMContext;
}

namespace prajna::ir {
class Module;
}

namespace prajna::codegen {

/// @brief  llvm的全局context, 若放在局部不是特别容易管理, 故直接搞成全局变量
// extern llvm::LLVMContext llvm_context;

std::shared_ptr<ir::Module> llvmCodegen(std::shared_ptr<ir::Module> ir_module,
                                        ir::Target ir_target);

}  // namespace prajna::codegen
