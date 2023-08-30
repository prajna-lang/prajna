#pragma once

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>

#include "boost/dll/runtime_symbol_info.hpp"
#include "prajna/assert.hpp"

#define RANGE(container) container.begin(), container.end()

#define POSITIONS(token) token.first_position, token.last_position

namespace prajna {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

class function_guard {
   public:
    static std::unique_ptr<function_guard> Create(std::function<void()> func) {
        auto self = std::unique_ptr<function_guard>(new function_guard);
        self->_todo = func;
        return self;
    }

    ~function_guard() { _todo(); }

   private:
    std::function<void()> _todo;
};

template <typename DstType_, typename SrcType_>
auto Cast(std::shared_ptr<SrcType_> ir_src) -> std::shared_ptr<DstType_> {
    auto ir_dst = std::dynamic_pointer_cast<DstType_>(ir_src);
    return ir_dst;
}

template <typename DstType_, typename SrcType_>
bool Is(std::shared_ptr<SrcType_> ir_src) {
    return Cast<DstType_, SrcType_>(ir_src) != nullptr;
}

template <typename Type>
inline auto Clone(Type t) -> std::decay_t<Type> {
    return t;
}

}  // namespace prajna
