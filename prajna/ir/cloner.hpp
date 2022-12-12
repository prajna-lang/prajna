#pragma once

#include <unordered_map>

// #include "prajna/ir/value.hpp"

namespace prajna::ir {

class Value;

class Cloner {
   public:
    std::unordered_map<std::shared_ptr<ir::Value>, std::shared_ptr<ir::Value>> value_dict;
};

}  // namespace prajna::ir
