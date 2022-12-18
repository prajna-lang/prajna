#include <list>
#include <map>
#include <set>

#include "prajna/ir/ir.hpp"
#include "prajna/transform/transform_pass.hpp"
#include "prajna/transform/utility.hpp"

namespace prajna::transform {

class MakeCompatiableWithLlvmPass : public FunctionPass {
   public:
    bool declaraExternalFunctionAndGlobalVariable(std::shared_ptr<ir::Function> ir_function) {
        bool changed = false;
        utility::eachValue(ir_function, [=, &changed](std::shared_ptr<ir::Value> ir_value) {
            if (auto ir_call = cast<ir::Call>(ir_value)) {
                auto ir_called_function_type = ir_call->function()->getFunctionType();
                PRAJNA_ASSERT(ir_called_function_type);
                auto ir_called_function = ir_called_function_type->function;
                PRAJNA_ASSERT(ir_called_function);
                if (ir_called_function->parent_module != ir_function->parent_module) {
                    ir_called_function->parent_module = ir_function->parent_module;
                    ir_function->parent_module->functions.push_front(ir_called_function);
                    changed = true;
                }
            }
        });

        return changed;
    }

    bool runOnFunction(std::shared_ptr<ir::Function> ir_function) override {
        bool changed = false;
        changed = this->declaraExternalFunctionAndGlobalVariable(ir_function) || changed;

        return changed;
    }
};

}  // namespace prajna::transform
