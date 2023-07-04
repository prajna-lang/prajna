#include "prajna/lowering/lower.h"

#include "prajna/lowering/interpreter_lowering_visitor.hpp"
#include "prajna/lowering/statement_lowering_visitor.hpp"
namespace prajna::lowering {

std::shared_ptr<ir::Module> lower(std::shared_ptr<ast::Statements> ast,
                                  std::shared_ptr<SymbolTable> symbol_table,
                                  std::shared_ptr<Logger> logger,
                                  std::shared_ptr<Compiler> compiler, bool is_interpreter) {
    auto ir_module = ir::Module::Create();
    if (is_interpreter) {
        auto interpreter_visitor =
            lowering::InterpreterLoweringVisitor::Create(symbol_table, ir_module, logger, compiler);
        return interpreter_visitor->Apply(ast);
    } else {
        auto statement_lowering_visitor =
            StatementLoweringVisitor::Create(symbol_table, logger, ir_module, compiler);
        return statement_lowering_visitor->Apply(ast);
    }
}

size_t InterpreterLoweringVisitor::_command_id = 0;

}  // namespace prajna::lowering
