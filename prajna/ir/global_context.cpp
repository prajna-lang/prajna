#include "prajna/ir/global_context.h"

#include "prajna/ir/type.hpp"

namespace prajna::ir {

GlobalContext::GlobalContext(size_t target_bits) {
    this->target_bits = target_bits;
    this->index_type = ir::IntType::create(target_bits, true);
}  // namespace prajna::ir

GlobalContext global_context = GlobalContext(64);

}  // namespace prajna::ir
