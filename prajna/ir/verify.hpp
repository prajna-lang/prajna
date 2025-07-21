#pragma once

#include <memory>

#include "prajna/helper.hpp"
#include "prajna/ir/ir.hpp"

namespace prajna::ir {

class VerifyVisitor : public ir::Visitor {
   protected:
    VerifyVisitor() = default;

   public:
    static std::shared_ptr<VerifyVisitor> Create() {
        auto self = std::shared_ptr<VerifyVisitor>(new VerifyVisitor);
        return self;
    }

    void Visit(std::shared_ptr<ir::Block> ir_block) override {
        for (auto ir_value : Clone(*ir_block)) {
            PRAJNA_VERIFY(Lock(ir_value->parent) == ir_block);
            ir_value->ApplyVisitor(this->shared_from_this());
        }
    }

    void Visit(std::shared_ptr<ir::Call> ir_call) override {
        // TODO(bug)
        // PRAJNA_VERIFY(Lock(ir_call->Function()->parent));
    }

    void Visit(std::shared_ptr<ir::If> ir_if) override {
        ir_if->TrueBlock()->ApplyVisitor(this->shared_from_this());
        ir_if->FalseBlock()->ApplyVisitor(this->shared_from_this());
    }

    void Visit(std::shared_ptr<ir::While> ir_while) override {
        ir_while->ConditionBlock()->ApplyVisitor(this->shared_from_this());
        ir_while->LoopBlock()->ApplyVisitor(this->shared_from_this());
    }

    void Visit(std::shared_ptr<ir::For> ir_for) override {
        ir_for->LoopBlock()->ApplyVisitor(this->shared_from_this());
    }

    void Visit(std::shared_ptr<ir::internal::ConditionBranch> ir_condition_branch) override {
        // 在SSA转换后, 条件分支的子块会在后面被访问
        // ir_condition_branch->TrueBlock()->ApplyVisitor(this->shared_from_this());
        // ir_condition_branch->FalseBlock()->ApplyVisitor(this->shared_from_this());
    }

    void Visit(std::shared_ptr<ir::internal::JumpBranch> ir_jump_branch) override {
        // ir_jump_branch->NextBlock()->ApplyVisitor(this->shared_from_this());
    }

    void Visit(std::shared_ptr<ir::Function> ir_function) override {
        for (auto ir_block : Clone(ir_function->blocks)) {
            PRAJNA_VERIFY(Lock(ir_block->parent) == ir_function);
            ir_block->ApplyVisitor(this->shared_from_this());
        }
    }

    void Visit(std::shared_ptr<ir::Module> ir_module) override {
        for (auto ir_function : Clone(ir_module->functions)) {
            PRAJNA_VERIFY(Lock(ir_function->parent) == ir_module);
            ir_function->ApplyVisitor(this->shared_from_this());
        }

        for (auto ir_global_variable : Clone(ir_module->global_variables)) {
            PRAJNA_VERIFY(Lock(ir_global_variable->parent) == ir_module);
        }
    }

   private:
    std::function<void(std::shared_ptr<ir::Value>)> callback_;
};

inline bool Verify(std::shared_ptr<ir::Value> ir_value) {
    PRAJNA_VERIFY(ir_value);
    auto visitor = VerifyVisitor::Create();
    ir_value->ApplyVisitor(visitor);
    return true;
}

}  // namespace prajna::ir
