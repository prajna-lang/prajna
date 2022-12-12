#pragma once

#include "boost/spirit/include/support_info.hpp"
#include "fmt/format.h"
#include "prajna/ast/ast.hpp"

namespace prajna::parser::grammar {

template <typename Iterator>
inline Iterator get_last_iterator(Iterator iter) {
    assert(iter != Iterator());
    auto iter_tmp = iter;
    while (iter != Iterator()) {
        iter_tmp = iter;
        ++iter;
    }

    return iter_tmp;
}

template <typename BaseIterator, typename Iterator>
struct issue_handler {
    template <typename, typename, typename>
    struct result {
        typedef void type;
    };

    void operator()(Iterator first, Iterator err_pos, boost::spirit::info what) const {
        Iterator err_pos1;
        if (err_pos == decltype(err_pos)()) {
            err_pos1 = get_last_iterator(first);
        } else {
            err_pos1 = err_pos;
        }
        BaseIterator err_pos_base = err_pos1->begin();
        auto pos = err_pos_base.get_position();
        std::stringstream ss;
        ss << what;
        std::string str =
            fmt::format("expect {}, In {}:{}:{}", ss.str(), pos.file, pos.line, pos.column);
        std::cout << str << std::endl;
    }
};

}  // namespace prajna::parser::grammar
