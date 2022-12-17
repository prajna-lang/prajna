#pragma once

#include "prajna/lowering/statement_lowering_visitor.hpp"

namespace prajna::lowering {

namespace detail {

ast::Identifier getIdentifier(ast::Operand ast_operand) {
    return boost::apply_visitor(
        overloaded{[=](auto x) -> ast::Identifier {
                       PRAJNA_TODO;
                       return ast::Identifier();
                   },
                   [](ast::KernelFunctionCall ast_kernel_function_call) {
                       if (ast_kernel_function_call.operation) {
                           return ast::Identifier();
                       } else {
                           return getIdentifier(ast_kernel_function_call.kernel_function);
                       }
                   },
                   [](ast::Expression ast_expression) {
                       if (ast_expression.rest.empty()) {
                           return getIdentifier(ast_expression.first);
                       } else {
                           return ast::Identifier();
                       }
                   },
                   [](ast::PostfixUnary ast_postfix) {
                       if (ast_postfix.operators.empty()) {
                           return getIdentifier(ast_postfix.operand);
                       } else {
                           return ast::Identifier();
                       }
                   },
                   [](ast::Identifier ast_identifier) { return ast_identifier; },
                   [](ast::IdentifiersResolution ast_identifiers_resolution) {
                       if (ast_identifiers_resolution.is_root) {
                           return ast::Identifier();
                       } else {
                           auto& identifiers = ast_identifiers_resolution.identifiers;
                           if (identifiers.size() != 1 || identifiers.front().template_arguments) {
                               return ast::Identifier();
                           }

                           return identifiers.front().identifier;
                       }
                   }},
        ast_operand);
}

}  // namespace detail

class InterpreterLoweringVisitor {
    InterpreterLoweringVisitor() = default;

   public:
    static std::shared_ptr<InterpreterLoweringVisitor> create(
        std::shared_ptr<lowering::SymbolTable> symbol_table, std::shared_ptr<Logger> logger) {
        std::shared_ptr<InterpreterLoweringVisitor> self(new InterpreterLoweringVisitor);
        self->_statement_lowering_visitor = StatementLoweringVisitor::create(symbol_table, logger);
        return self;
    }

    void operator()(const ast::Statement& x) { boost::apply_visitor(*this, x); }

    void operator()(ast::Statements const& x) {
        for (auto stat : x) {
            (*this)(stat);
        }
    }

    std::shared_ptr<ir::Module> apply(std::shared_ptr<ast::Statements> sp_ast_statements) {
        (*this)(*sp_ast_statements);
        return _statement_lowering_visitor->ir_utility->module;
    }

    std::unique_ptr<function_guard> wrapCommandLineWithFunction() {
        auto ir_fun_type = ir::FunctionType::create(ir::VoidType::create(), {});
        auto ir_function = ir::Function::create(ir_fun_type);
        ir_function->function_type->annotations.insert({"\\command", {}});
        ir_function->name = "\\command" + std::to_string(_command_id++);
        ir_function->fullname = ir_function->name;
        ir_function->parent_module = _statement_lowering_visitor->ir_utility->module;
        _statement_lowering_visitor->ir_utility->module->functions.push_back(ir_function);

        auto ir_utility = _statement_lowering_visitor->ir_utility;
        auto ir_block = ir::Block::create();
        ir_block->parent_function = ir_function;
        ir_utility->ir_current_block = ir_block;
        ir_function->blocks.push_back(ir_block);

        return function_guard::create([&]() {
            // 打印结果
            auto ir_utility = _statement_lowering_visitor->ir_utility;
            boost::apply_visitor(
                overloaded{[&](auto x) {},
                           [&](std::shared_ptr<ir::Value> ir_result_value) {
                               if (!ir_result_value->type->member_functions["tostr"]) return;
                               auto ir_result_string =
                                   ir_utility->callMemberFunction(ir_result_value, "tostr", {});
                               ir_utility->callMemberFunction(ir_result_string, "print", {});
                           }},
                _symbol_result);

            ir_utility->create<ir::Return>(ir::VoidValue::create());
            ir_utility->ir_current_block = nullptr;
        });
    }

    void operator()(const ast::VariableDeclaration& ast_variable_declaration) {
        auto wrap_function_guard = this->wrapCommandLineWithFunction();
        _symbol_result = (*_statement_lowering_visitor)(ast_variable_declaration, true);
    }

    void operator()(const ast::Blank) { return; }

    void operator()(const ast::Assignment& ast_assignment) {
        auto wrap_function_guard = this->wrapCommandLineWithFunction();
        auto ir_utility = _statement_lowering_visitor->ir_utility;

        // 需要处理一下
        std::string identifier = detail::getIdentifier(ast_assignment.left);
        if (identifier.empty() || ir_utility->symbol_table->has(identifier)) {
            _symbol_result = (*_statement_lowering_visitor)(ast_assignment);
        } else {
            auto symbol_rhs = _statement_lowering_visitor->expression_lowering_visitor->operator()(
                ast_assignment.right);
            auto ir_rhs = symbolGet<ir::Value>(symbol_rhs);
            PRAJNA_ASSERT(ir_rhs);

            auto ir_global_variable = ir::GlobalVariable::create(ir_rhs->type);
            ir_global_variable->parent_module = ir_utility->module;
            ir_utility->module->global_variables.push_back(ir_global_variable);
            ir_global_variable->name = identifier;
            ir_global_variable->fullname =
                concatFullname(ir_utility->symbol_table->fullname(), ir_global_variable->name);
            ir_utility->symbol_table->set(ir_global_variable, identifier);
            ir_utility->create<ir::WriteVariableLiked>(ir_rhs, ir_global_variable);
            _symbol_result = ir_rhs;
        }
    }

    void operator()(const ast::Return x) { PRAJNA_TODO; }
    void operator()(const ast::Continue x) { PRAJNA_TODO; }
    void operator()(const ast::Break x) { PRAJNA_TODO; }

    void operator()(const ast::Function x) { (*_statement_lowering_visitor)(x); }
    void operator()(const ast::Struct x) { (*_statement_lowering_visitor)(x); }
    void operator()(const ast::ImplementStruct x) { (*_statement_lowering_visitor)(x); }
    void operator()(const ast::Interface x) { (*_statement_lowering_visitor)(x); }
    void operator()(const ast::ImplementStructForInterface x) { (*_statement_lowering_visitor)(x); }

    template <typename T>
    void operator()(const T& t) {
        auto wrap_function_guard = this->wrapCommandLineWithFunction();
        _symbol_result = (*_statement_lowering_visitor)(t);
    }

   private:
    Symbol _symbol_result = nullptr;
    std::shared_ptr<StatementLoweringVisitor> _statement_lowering_visitor = nullptr;
    static size_t _command_id;
};

}  // namespace prajna::lowering
