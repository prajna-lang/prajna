#include "prajna/lowering/lower.h"

#include "prajna/lowering/interpreter_lowering_visitor.hpp"
#include "prajna/lowering/statement_lowering_visitor.hpp"
namespace prajna::lowering {

std::shared_ptr<ir::Module> lower(std::shared_ptr<ast::Statements> ast,
                                  std::shared_ptr<SymbolTable> symbol_table,
                                  std::shared_ptr<Logger> logger,
                                  std::shared_ptr<Compiler> compiler, bool is_interpreter) {
    if (is_interpreter) {
        auto interpreter_visitor =
            lowering::InterpreterLoweringVisitor::create(symbol_table, logger, compiler);
        return interpreter_visitor->apply(ast);
    } else {
        auto statement_lowering_visitor =
            StatementLoweringVisitor::create(symbol_table, logger, nullptr, compiler);
        return statement_lowering_visitor->apply(ast);
    }
}

size_t InterpreterLoweringVisitor::_command_id = 0;

}  // namespace prajna::lowering
