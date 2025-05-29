#include "prajna/lowering/expression_lowering_visitor.hpp"

#include "prajna/lowering/statement_lowering_visitor.hpp"

namespace prajna::lowering {

std::shared_ptr<ir::Value> ExpressionLoweringVisitor::operator()(ast::Closure ast_closure) {
    // TODO: 需要修复嵌套情况下的closure
    auto ir_closure_struct_type = ir::StructType::Create();
    this->ir_builder->current_implement_type = ir_closure_struct_type;
    ir_builder->PushSymbolTable();
    ir_builder->symbol_table->name = "closure." + std::to_string(ir_builder->closure_id);
    ++ir_builder->closure_id;
    auto guard = ScopeExit::Create([this]() {
        this->ir_builder->PopSymbolTable();
        this->ir_builder->current_implement_type = nullptr;
    });

    // ast_closure
    ast::Function ast_function_tmp;
    ast_function_tmp.declaration.name = "__call__";
    ast_function_tmp.declaration.parameters = ast_closure.parameters;
    ast_function_tmp.declaration.return_type_optional = ast_closure.return_type_optional;
    ast_function_tmp.body_optional = ast_closure.body;

    auto statement_lowering_visitor = wp_statement_lowering_visitor.lock();
    PRAJNA_ASSERT(statement_lowering_visitor);

    auto ir_closure_function =
        Cast<ir::Function>(SymbolGet<ir::Value>((*statement_lowering_visitor)(ast_function_tmp)));
    auto ir_closure = this->ir_builder->Create<ir::LocalVariable>(ir_closure_struct_type);
    ir_closure_function->closure = ir_closure;
    return ir_closure;
}

}  // namespace prajna::lowering
