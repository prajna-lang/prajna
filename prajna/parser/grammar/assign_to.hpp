#pragma once

#include "boost/spirit/include/qi.hpp"
#include "prajna/assert.hpp"
#include "prajna/ast/ast.hpp"
#include "prajna/exception.hpp"

namespace boost::spirit::traits {

namespace {

template <typename Iterator>
prajna::ast::SourcePosition get_source_position(const Iterator& iter) {
    auto pos = iter.get_position();
    return prajna::ast::SourcePosition{pos.line, pos.column, pos.file};
}

template <typename Iterator>
inline char get_escape_char(Iterator iter) {
    auto c = *iter;
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
            prajna::InvalidEscapeChar lexer_error;
            lexer_error.source_position = get_source_position(iter);
            throw lexer_error;
    }

    return '\0';
}

}  // namespace

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::Blank, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::Blank& attr) {
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::SourceLocation, Iterator> {
    static void call(const Iterator& first, const Iterator& last,
                     prajna::ast::SourceLocation& attr) {
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::Identifier, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::Identifier& attr) {
        // attr = prajna::ast::Identifier(first, last);
        auto str = std::string(first, last);
        attr = prajna::ast::Identifier(str);
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::Operator, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::Operator& attr) {
        attr.string_token = std::string(first, last);
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::Break, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::Break& attr) {
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::Continue, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::Continue& attr) {
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::CharLiteral, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::CharLiteral& attr) {
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);

        if (last == std::next(first, 3)) {
            attr.value = *std::next(first);
            return;
        }

        if (last == std::next(first, 4)) {
            PRAJNA_ASSERT(*std::next(first) == '\\');
            attr.value = get_escape_char(std::next(first, 2));
            return;
        }

        PRAJNA_ASSERT(false);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::StringLiteral, Iterator> {
    static void call(const Iterator& first, const Iterator& last,
                     prajna::ast::StringLiteral& attr) {
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);

        auto tmp = std::string(first, last);
        for (auto it = std::next(first); std::next(it) != last; ++it) {
            if (*it != '\\') {
                attr.value.push_back(*it);
            } else {
                ++it;
                if (std::next(it) == last) {
                    throw prajna::InvalidEscapeChar{get_source_position(it)};
                }
                attr.value.push_back(get_escape_char(it));
            }
        }
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::BoolLiteral, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::BoolLiteral& attr) {
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);

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
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);

        qi::parse(first, last, int_type(), attr.value);
        // qi::parse(first, last, int_type(), attr.mp_int_value);
    }
};

template <typename Iterator>
struct assign_to_attribute_from_iterators<prajna::ast::FloatLiteral, Iterator> {
    static void call(const Iterator& first, const Iterator& last, prajna::ast::FloatLiteral& attr) {
        attr.first_position = get_source_position(first);
        attr.last_position = get_source_position(last);

        qi::parse(first, last, float_type(), attr.value);
    }
};

template <>
struct assign_to_container_from_value<prajna::ast::Identifier, prajna::ast::StringLiteral> {
    static void call(prajna::ast::StringLiteral ast_string_literal,
                     prajna::ast::Identifier& ast_identifier) {
        ast_identifier = ast_string_literal.value;
        ast_identifier.first_position = ast_string_literal.first_position;
        ast_identifier.last_position = ast_string_literal.last_position;
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

// template <typename Attribute, typename Iterator, typename AttributeTypes, typename HasState,
//           typename Idtype>
// struct assign_to_attribute_from_value<
//     prajna::ast::Blank, lex::lexertl::token<Iterator, AttributeTypes, HasState, Idtype>> {
//     static void call(lex::lexertl::token<Iterator, AttributeTypes, HasState, Idtype> const& t,
//                      prajna::ast::Blank& attr) {}  // namespace traits
}  // namespace boost::spirit::traits
