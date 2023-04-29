
#include <functional>
#include <iostream>

namespace prajna {

/// 在不同的IDE中, 需要重载合适的实现
std::function<void(std::string)> print_callback = [](std::string str) { std::cout << str; };
std::string __input;

/// 在不同的IDE中, 需要重载不同的实现
std::function<char *(void)> input_callback = []() -> char * {
    __input.clear();
    while (true) {
        auto c = getchar();
        if (c == EOF || c == 10 || c == 4) {
            break;
        }
        __input.push_back(static_cast<char>(c));
    }
    return (char *)__input.c_str();
};

}  // namespace prajna
