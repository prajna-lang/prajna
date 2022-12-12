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

namespace lowering {

std::shared_ptr<ir::Module> lower(std::shared_ptr<ast::Statements> ast,
                                  std::shared_ptr<SymbolTable> symbol_table,
                                  std::shared_ptr<Logger> logger, bool is_interpreter);
}  // namespace lowering
}  // namespace prajna
