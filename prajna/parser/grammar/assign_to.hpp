#pragma once

#include "boost/spirit/include/qi.hpp"
#include "prajna/ast/ast.hpp"

namespace boost {
namespace spirit {
namespace traits {

namespace {

inline char get_escape_char(char c) {
    switch (c) {
        case 'a': {
            return '\a';
        }
        case 'b': {
            return '\b';
        }
        case 'e': {
            return '\e';
        }
        case 'f': {
            return '\f';
        }
        case 'n': {
            return '\n';
        }
        case 'r': {
            return '\r';
        }
        case 't': {
            return '\t';
        }
        case 'v': {
            return '\v';
        }
        case '\\': {
            return '\\';
        }
        case '\'': {
            return '\'';
        }
        case '\0': {
            return '\0';
        }
        // mac和linux有区别
        case '0': {
            return '\0';
        }
        case '\"': {
            return '\"';
        }
        default:
            assert(false);  /// todo
    }

    return '\0';
}

template <typename Iterator>
prajna::ast::SourcePosition get_source_position(const Iterator& first) {
    auto pos = first.get_position();
    return prajna::ast::SourcePosition{pos.line, pos.column, pos.file};
}

}  // namespace

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::AstBase, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::AstBase& ast_base) {
        ast_base.position = get_source_position(first);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::Identifier, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::Identifier& attr) {
        // attr = prajna::ast::Identifier(first, last);
        auto str = std::string(first, last);
        attr = prajna::ast::Identifier(str);
        attr.position = get_source_position(first);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::Operator, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::Operator& attr) {
        attr.position = get_source_position(first);
        attr.string_token = std::string(first, last);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::CharLiteral, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::CharLiteral& attr) {
        attr.position = get_source_position(first);

        auto tmp = std::string(first, last);
        auto first1 = tmp.begin();
        auto last1 = tmp.end();
        if ((last1 - first1) == 3) {
            attr.value = *(first1 + 1);
            return;
        }
        if ((last1 - first1) == 4) {
            assert(*(first1 + 1) == '\\');  /// todo
            attr.value = get_escape_char(*(first1 + 2));
            return;
        }

        assert(false);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::StringLiteral, Iterator> {
    static void call(const Iterator& first, const Iterator& last,
                     prajna::ast::StringLiteral& attr) {
        attr.position = get_source_position(first);

        auto tmp = std::string(first, last);
        for (auto it = tmp.begin() + 1; it != tmp.end() - 1; ++it) {
            if (*it != '\\') {
                attr.value.push_back(*it);
            } else {
                ++it;
                attr.value.push_back(get_escape_char(*it));
            }
        }
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::BoolLiteral, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::BoolLiteral& attr) {
        attr.position = get_source_position(first);

        auto tmp = std::string(first, last);
        if (tmp == "true") {
            attr.value = true;
            return;
        }
        if (tmp == "false") {
            attr.value = false;
            return;
        }
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::IntLiteral, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::IntLiteral& attr) {
        attr.position = get_source_position(first);

        qi::parse(first, last, int_type(), attr.value);
        // qi::parse(first, last, int_type(), attr.mp_int_value);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::FloatLiteral, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::FloatLiteral& attr) {
        attr.position = get_source_position(first);

        qi::parse(first, last, float_type(), attr.value);
    }
};

template <>
struct assign_to_container_from_value<prajna::ast::Identifier, prajna::ast::StringLiteral> {
    static void call(prajna::ast::StringLiteral ast_string_literal,
                     prajna::ast::Identifier& ast_identifier) {
        ast_identifier = ast_string_literal.value;
        ast_identifier.position = ast_string_literal.position;
    }
};

template <>
struct assign_to_container_from_value<std::string, prajna::ast::StringLiteral> {
    static void call(prajna::ast::StringLiteral string_literal, std::string& attr) {
        attr = string_literal.value;
    }
};

template <>
struct assign_to_container_from_value<prajna::ast::Identifier, prajna::ast::Operator> {
    static void call(prajna::ast::Operator op, prajna::ast::Identifier& attr) {
        attr = prajna::ast::Identifier(op.string_token);
    }
};

template <>
struct assign_to_container_from_value<prajna::ast::Identifier, std::vector<prajna::ast::Operator>> {
    static void call(std::vector<prajna::ast::Operator> ops, prajna::ast::Identifier& identifier) {
        std::string tmp;
        for (auto tmp_op : ops) {
            tmp.append(tmp_op.string_token);
        }
        identifier = prajna::ast::Identifier(tmp);
    }
};

}  // namespace traits
}  // namespace spirit
}  // namespace boost
