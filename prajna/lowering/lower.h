#pragma once

#include <memory>

#include "prajna/lowering/symbol_table.hpp"

namespace prajna {

namespace ir {
class Module;
}
namespace ast {
class Statements;
}
class Logger;

class Compiler;

namespace lowering {

std::shared_ptr<ir::Module> lower(std::shared_ptr<ast::Statements> ast,
                                  std::shared_ptr<SymbolTable> symbol_table,
                                  std::shared_ptr<Logger> logger,
                                  std::shared_ptr<Compiler> compiler, bool is_interpreter);
}  // namespace lowering
}  // namespace prajna
