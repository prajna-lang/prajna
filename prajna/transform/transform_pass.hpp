#pragma once

namespace prajna::ir {

class Function;

}

namespace prajna::transform {

class FunctionPass {
   public:
    virtual bool RunOnFunction(std::shared_ptr<ir::Function> ir_function) = 0;
};

}  // namespace prajna::transform
