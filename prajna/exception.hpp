#pragma once

#include <memory>

namespace prajna {

class CompileError {
   protected:
    CompileError() = default;

   public:
    static std::shared_ptr<CompileError> create() {
        std::shared_ptr<CompileError> self(new CompileError);
        return self;
    }
};

}  // namespace prajna
