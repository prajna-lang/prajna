#pragma once

#include <list>
#include <memory>
#include <vector>

namespace prajna::ir {

class Type;

class GlobalContext {
   public:
    GlobalContext(int64_t target_bits);

    void Reset();

    /// @brief 用于存储已经构造了的类型
    /// @note 需要使用vector来确保构造的顺序, 因为后面的codegen需要顺序正确
    std::list<std::shared_ptr<Type>> created_types;
};

extern GlobalContext global_context;

extern std::shared_ptr<Type> f16;
extern std::shared_ptr<Type> f32;
extern std::shared_ptr<Type> f64;
extern std::shared_ptr<Type> i8;
extern std::shared_ptr<Type> i16;
extern std::shared_ptr<Type> i32;
extern std::shared_ptr<Type> i64;
extern std::shared_ptr<Type> u8;
extern std::shared_ptr<Type> u16;
extern std::shared_ptr<Type> u32;
extern std::shared_ptr<Type> u64;
extern std::shared_ptr<Type> bool_;
extern std::shared_ptr<Type> char_;
extern std::shared_ptr<Type> void_;

}  // namespace prajna::ir
