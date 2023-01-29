#pragma once

#include <memory>
#include <vector>

namespace prajna::ir {

class Type;

class GlobalContext {
   public:
    GlobalContext(size_t target_bits);

    /// @brief 用于存储已经构造了的类型
    /// @note 需要使用vector来确保构造的顺序, 因为后面的codegen需要顺序正确
    std::vector<std::shared_ptr<Type>> created_types;
};

extern GlobalContext global_context;

}  // namespace prajna::ir
