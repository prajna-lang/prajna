
#include <functional>
#include <iostream>

namespace prajna {

std::function<void(std::string)> print_callback = [](std::string str) { std::cout << str; };

}  // namespace prajna
