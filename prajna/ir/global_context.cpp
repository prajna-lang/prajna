#include "prajna/ir/global_context.h"

#include "prajna/ir/type.hpp"

namespace prajna::ir {

GlobalContext::GlobalContext(int64_t target_bits) {}  // namespace prajna::ir

GlobalContext global_context = GlobalContext(64);

}  // namespace prajna::ir
