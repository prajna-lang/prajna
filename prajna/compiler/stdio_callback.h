#pragma once

#include <functional>

namespace prajna {

extern std::function<void(std::string)> print_callback;
extern std::function<char*(void)> input_callback;

}  // namespace prajna
