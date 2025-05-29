#pragma once

#include <memory>

#include "prajna/ir/callback_visitor.hpp"

namespace prajna::transform {

class EachTensorVisitor : public ir::CallbackVisitor {
   public:
    EachTensorVisitor(std::function<void(std::shared_ptr<ir::Value>)> callback)
        : ir::CallbackVisitor(callback) {}

    static std::shared_ptr<EachTensorVisitor> Create(
        std::function<void(std::shared_ptr<ir::Value>)> callback) {
        auto self = std::shared_ptr<EachTensorVisitor>(new EachTensorVisitor(callback));
        return self;
    }

    void Visit(std::shared_ptr<ir::Block> ir_block) override {
        this->callback(ir_block);
        for (auto ir_value : Clone(*ir_block)) {  // Each的过程中会修改
            ir_value->ApplyVisitor(this->shared_from_this());
        }
    }

    void Visit(std::shared_ptr<ir::If> ir_if) override {
        this->callback(ir_if);
        ir_if->TrueBlock()->ApplyVisitor(this->shared_from_this());
        ir_if->FalseBlock()->ApplyVisitor(this->shared_from_this());
    }

    void Visit(std::shared_ptr<ir::While> ir_while) override {
        this->callback(ir_while);

        ir_while->ConditionBlock()->ApplyVisitor(this->shared_from_this());
        ir_while->LoopBlock()->ApplyVisitor(this->shared_from_this());
    }

    void Visit(std::shared_ptr<ir::For> ir_for) override {
        this->callback(ir_for);
        ir_for->LoopBlock()->ApplyVisitor(this->shared_from_this());
    }

    void Visit(std::shared_ptr<ir::Function> ir_function) override {
        this->callback(ir_function);
        for (auto ir_block : Clone(ir_function->blocks)) {  // Each的过程中会修改
            ir_block->parent = ir_function;
            ir_block->ApplyVisitor(this->shared_from_this());
        }
    }

    void Visit(std::shared_ptr<ir::Module> ir_module) override {
        // this->callback(ir_module);
        for (auto ir_function : Clone(ir_module->functions)) {  // Each的过程中会修改
            ir_function->parent = ir_module;
            ir_function->ApplyVisitor(this->shared_from_this());
        }

        for (auto ir_global_variable : Clone(ir_module->global_variables)) {  // Each的过程中会修改
            this->callback(ir_global_variable);
        }
    }
};

template <typename Value_>
inline void Each(std::shared_ptr<ir::Value> ir_tensor,
                 std::function<void(std::shared_ptr<Value_>)> callback) {
    auto visitor = EachTensorVisitor::Create([=](std::shared_ptr<ir::Value> ir_tensor) {
        if (auto ir_value = Cast<Value_>(ir_tensor)) {
            callback(ir_value);
        }
    });
    ir_tensor->ApplyVisitor(visitor);
}

}  // namespace prajna::transform
