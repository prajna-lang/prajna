#include "prajna/ir/global_context.h"

#include "prajna/ir/type.hpp"

namespace prajna::ir {

GlobalContext::GlobalContext(int64_t target_bits) {}

void GlobalContext::Reset() {
    created_types.clear();
    f16 = FloatType::Create(16);
    f32 = FloatType::Create(32);
    f64 = FloatType::Create(64);
    i8 = IntType::Create(8, true);
    i16 = IntType::Create(16, true);
    i32 = IntType::Create(32, true);
    i64 = IntType::Create(64, true);
    u8 = IntType::Create(8, false);
    u16 = IntType::Create(16, false);
    u32 = IntType::Create(32, false);
    u64 = IntType::Create(64, false);
    bool_ = BoolType::Create();
    char_ = CharType::Create();
    void_ = VoidType::Create();
}

GlobalContext global_context = GlobalContext(64);

std::shared_ptr<Type> f16(FloatType::Create(16));
std::shared_ptr<Type> f32(FloatType::Create(32));
std::shared_ptr<Type> f64(FloatType::Create(64));
std::shared_ptr<Type> i8(IntType::Create(8, true));
std::shared_ptr<Type> i16(IntType::Create(16, true));
std::shared_ptr<Type> i32(IntType::Create(32, true));
std::shared_ptr<Type> i64(IntType::Create(64, true));
std::shared_ptr<Type> u8(IntType::Create(8, false));
std::shared_ptr<Type> u16(IntType::Create(16, false));
std::shared_ptr<Type> u32(IntType::Create(32, false));
std::shared_ptr<Type> u64(IntType::Create(64, false));
std::shared_ptr<Type> bool_(BoolType::Create());
std::shared_ptr<Type> char_(CharType::Create());
std::shared_ptr<Type> void_(VoidType::Create());

}  // namespace prajna::ir
