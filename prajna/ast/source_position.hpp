#pragma once

namespace prajna::ast {

struct SourcePosition {
    int line = -1;
    int column = -1;
    std::string file;
};

}  // namespace prajna::ast
