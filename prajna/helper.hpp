#pragma once

#include <functional>
#include <memory>

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
    static std::unique_ptr<function_guard> create(std::function<void()> func) {
        auto self = std::unique_ptr<function_guard>(new function_guard);
        self->_todo = func;
        return self;
    }

    ~function_guard() { _todo(); }

   private:
    std::function<void()> _todo;
};

template <typename DstType_, typename SrcType_>
auto cast(std::shared_ptr<SrcType_> ir_src) -> std::shared_ptr<DstType_> {
    auto ir_dst = std::dynamic_pointer_cast<DstType_>(ir_src);
    return ir_dst;
}

template <typename DstType_, typename SrcType_>
bool is(std::shared_ptr<SrcType_> ir_src) {
    return cast<DstType_, SrcType_>(ir_src) != nullptr;
}

template <typename _T>
using sp = std::shared_ptr<_T>;

}  // namespace prajna
