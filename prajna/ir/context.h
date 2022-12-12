#pragma once

#include <memory>
#include <vector>

namespace prajna::ir {

class Type;

class Context {
   public:
    Context(size_t target_bits) { this->target_bits = target_bits; }

    /// @brief 用于存储已经构造了的类型
    /// @note 需要使用vector来确保构造的顺序, 因为后面的codegen需要顺序正确
    std::vector<std::shared_ptr<Type>> created_types;
    size_t target_bits = 64;
};

extern Context context;

}  // namespace prajna::ir
