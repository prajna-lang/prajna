
#include <functional>
#include <iostream>

namespace prajna {

std::function<void(std::string)> print_callback = [](std::string str) { std::cout << str; };
std::string __input;
std::function<char *(void)> input_callback = []() -> char * {
    __input.clear();
    // auto c = getchar();
    while (true) {
        auto c = getchar();
        if (c == EOF || c == 10) {
            break;
        }
        __input.push_back(static_cast<char>(c));
    }
    return (char *)__input.c_str();
};

}  // namespace prajna
