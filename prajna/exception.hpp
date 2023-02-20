#pragma once

#pragma once

#include "prajna/ast/source_position.hpp"

namespace prajna {

struct InvalidEscapeChar {
    ast::SourcePosition source_position;
};

class CompileError {
   public:
    CompileError() = default;
};

class RuntimeError {
   public:
    RuntimeError() = default;
};

}  // namespace prajna
