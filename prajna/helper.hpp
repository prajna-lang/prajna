#pragma once

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <ranges>

#include "boost/dll/runtime_symbol_info.hpp"
#include "prajna/assert.hpp"

#define POSITIONS(token) token.first_position, token.last_position

namespace prajna {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

class ScopeExit {
   public:
    static std::unique_ptr<ScopeExit> Create(std::function<void()> func) {
        auto self = std::unique_ptr<ScopeExit>(new ScopeExit);
        self->_todo = func;
        return self;
    }

    ~ScopeExit() { _todo(); }

   private:
    std::function<void()> _todo;
};

template <typename DstType_, typename SrcType_>
auto Cast(std::shared_ptr<SrcType_> ir_src) -> std::shared_ptr<DstType_> {
    auto ir_dst = std::dynamic_pointer_cast<DstType_>(ir_src);
    return ir_dst;
}

template <typename DstType_, typename SrcType_>
auto ListCast(std::list<std::shared_ptr<SrcType_>> src_list)
    -> std::list<std::shared_ptr<DstType_>> {
    std::list<std::shared_ptr<DstType_>> dst_list;
    std::ranges::transform(src_list, std::back_inserter(dst_list),
                           [](std::shared_ptr<SrcType_> x) { return Cast<DstType_>(x); });
    return dst_list;
}

template <typename DstType_, typename SrcType_>
bool Is(std::shared_ptr<SrcType_> ir_src) {
    return Cast<DstType_, SrcType_>(ir_src) != nullptr;
}

template <typename Type>
inline auto Clone(Type t) -> std::decay_t<Type> {
    return t;
}

template <typename T>
std::shared_ptr<T> Lock(std::weak_ptr<T>& weak) {
    PRAJNA_ASSERT(!weak.expired());
    auto ptr = weak.lock();
    PRAJNA_ASSERT(ptr);
    return ptr;
}

}  // namespace prajna
