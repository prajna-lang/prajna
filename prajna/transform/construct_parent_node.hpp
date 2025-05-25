#pragma once

#include <memory>

#include "prajna/helper.hpp"
#include "prajna/ir/ir.hpp"

namespace prajna::transform {

class ConstructParentNodeVisitor : public ir::Visitor {
   protected:
    ConstructParentNodeVisitor() = default;

   public:
    static std::shared_ptr<ConstructParentNodeVisitor> Create() {
        auto self = std::shared_ptr<ConstructParentNodeVisitor>(new ConstructParentNodeVisitor);
        return self;
    }

    void Visit(std::shared_ptr<ir::Block> ir_block) override {
        for (auto ir_value : Clone(*ir_block)) {
            ir_value->parent = ir_block;
            ir_value->ApplyVisitor(this->shared_from_this());
        }
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

    void Visit(std::shared_ptr<ir::Function> ir_function) override {
        for (auto ir_block : Clone(ir_function->blocks)) {
            ir_block->parent = ir_function;
            ir_block->ApplyVisitor(this->shared_from_this());
        }
    }

    void Visit(std::shared_ptr<ir::Module> ir_module) override {
        for (auto ir_function : Clone(ir_module->functions)) {
            ir_function->parent_module = ir_module;
            ir_function->ApplyVisitor(this->shared_from_this());
        }

        for (auto ir_global_variable : Clone(ir_module->global_variables)) {
            ir_global_variable->parent_module = ir_module;
        }
    }

   private:
    std::function<void(std::shared_ptr<ir::Value>)> callback_;
};

}  // namespace prajna::transform
