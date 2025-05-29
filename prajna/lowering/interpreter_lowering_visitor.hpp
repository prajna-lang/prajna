#pragma once

#include "prajna/compiler/compiler.h"
#include "prajna/lowering/statement_lowering_visitor.hpp"

namespace prajna::lowering {

class InterpreterLoweringVisitor {
    InterpreterLoweringVisitor() = default;

   public:
    static std::shared_ptr<InterpreterLoweringVisitor> Create(
        std::shared_ptr<lowering::SymbolTable> symbol_table, std::shared_ptr<ir::Module> ir_module,
        std::shared_ptr<Logger> logger, std::shared_ptr<Compiler> compiler) {
        std::shared_ptr<InterpreterLoweringVisitor> self(new InterpreterLoweringVisitor);
        self->logger = logger;
        self->compiler = compiler;
        self->_statement_lowering_visitor =
            StatementLoweringVisitor::Create(symbol_table, logger, ir_module, compiler);
        return self;
    }

    void operator()(const ast::Statement& x) { boost::apply_visitor(*this, x); }

    void operator()(ast::Statements const& x) {
        for (auto stat : x) {
            (*this)(stat);
        }
    }

    std::shared_ptr<ir::Module> Apply(std::shared_ptr<ast::Statements> sp_ast_statements) {
        // 不能直接调用_statement_lowering_visitor->apply, 需要wrap
        (*this)(*sp_ast_statements);
        auto ir_module = _statement_lowering_visitor->ir_builder->module;
        ir_module->symbol_table = _statement_lowering_visitor->ir_builder->symbol_table;
        return _statement_lowering_visitor->ir_builder->module;
    }

    std::unique_ptr<ScopeExit> WrapCommandLineWithFunction() {
        auto ir_fun_type = ir::FunctionType::Create({}, ir::VoidType::Create());
        auto ir_function = ir::Function::Create(ir_fun_type);
        ir_function->annotation_dict.insert({"\\command", {}});
        ir_function->name = "\\command" + std::to_string(_command_id++);
        ir_function->fullname = ir_function->name;
        ir_function->parent = _statement_lowering_visitor->ir_builder->module;
        _statement_lowering_visitor->ir_builder->module->functions.push_back(ir_function);

        auto ir_builder = _statement_lowering_visitor->ir_builder;
        auto ir_block = ir::Block::Create();
        ir_block->parent = ir_function;
        ir_builder->PushBlock(ir_block);
        ir_builder->function_stack.push(ir_function);
        ir_function->blocks.push_back(ir_block);

        // 打印结果
        return ScopeExit::Create([=, ir_builder = _statement_lowering_visitor->ir_builder]() {
            boost::apply_visitor(
                overloaded{[=](auto x) {},
                           [=](std::shared_ptr<ir::Value> ir_result_value) {
                               if (!ir_result_value->type || !compiler->settings.print_result)
                                   return;

                               if (!ir_builder->GetMemberFunction(ir_result_value->type,
                                                                  "ToString"))
                                   return;
                               auto ir_result_string =
                                   ir_builder->CallMemberFunction(ir_result_value, "ToString", {});
                               ir_builder->CallMemberFunction(ir_result_string, "Print", {});
                           }},
                _symbol_printing_result);

            ir_builder->Create<ir::Return>(ir_builder->Create<ir::VoidValue>());
            ir_builder->function_stack.pop();
            ir_builder->PopBlock();
        });
    }

    void operator()(const ast::VariableDeclaration& ast_variable_declaration) {
        auto wrap_function_guard = this->WrapCommandLineWithFunction();
        (*_statement_lowering_visitor)(ast_variable_declaration, true);
    }

    void operator()(const ast::Expression& ast_expression) {
        auto wrap_function_guard = this->WrapCommandLineWithFunction();
        _symbol_printing_result = (*_statement_lowering_visitor)(ast_expression);
    }

    void operator()(const ast::Blank) { return; }

    void operator()(const ast::Return x) { logger->Error("\"return\" not in a function", x); }
    void operator()(const ast::Continue x) { logger->Error("\"continue\" not in a loop", x); }
    void operator()(const ast::Break x) { logger->Error("\"break\" not in a loop", x); }

    void operator()(const ast::Function x) { (*_statement_lowering_visitor)(x); }
    void operator()(const ast::Struct x) { (*_statement_lowering_visitor)(x); }
    void operator()(const ast::InterfacePrototype x) { (*_statement_lowering_visitor)(x); }
    void operator()(const ast::Template x) { (*_statement_lowering_visitor)(x); }
    void operator()(const ast::Use x) { (*_statement_lowering_visitor)(x); }
    void operator()(const ast::ImplementType x) { (*_statement_lowering_visitor)(x); }

    template <typename T>
    void operator()(const T& t) {
        auto wrap_function_guard = this->WrapCommandLineWithFunction();
        (*_statement_lowering_visitor)(t);
    }

   private:
    Symbol _symbol_printing_result = nullptr;
    std::shared_ptr<StatementLoweringVisitor> _statement_lowering_visitor = nullptr;
    static int64_t _command_id;
    std::shared_ptr<Logger> logger;
    std::shared_ptr<Compiler> compiler;
};

}  // namespace prajna::lowering
