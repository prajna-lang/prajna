#pragma once

#include "boost/spirit/include/support_info.hpp"
#include "fmt/format.h"
#include "prajna/assert.hpp"
#include "prajna/ast/ast.hpp"
#include "prajna/logger.hpp"
#include "prajna/parser/lexer/code_lexer_config.hpp"

namespace prajna::parser::grammar {

namespace {

template <typename _Iterator>
inline ast::SourcePosition getFirstIteratorPosition(_Iterator first_iter) {
    auto pos = first_iter->begin().get_position();
    return ast::SourcePosition{pos.line, pos.column, pos.file};
}

template <typename _Iterator>
inline ast::SourcePosition getLastIteratorPosition(_Iterator first_iter, _Iterator last_iter) {
    auto iter = first_iter;
    auto iter_tmp = iter;
    // 存在iter > last_iter的情况, 匹配annotations的时候就存在, 因为他们会匹配"空"的,
    // 会导致这种情况存在 while (iter != last_iter) {
    while (iter < last_iter) {
        iter_tmp = iter;
        ++iter;
    }
    // 代码有换行符,其不参与语法解析, 所以该位置肯定不会是最后一个字符
    auto pos = iter_tmp->end().get_position();
    return ast::SourcePosition{pos.line, pos.column, pos.file};
}

}  // namespace

template <typename _Iterator>
struct ErrorHandler {
    ErrorHandler(std::shared_ptr<Logger> logger) { this->logger = logger; }

    /// @brief
    /// @param first
    /// @param input_last 指向输入的末尾, 且无意义
    /// @param err_pos
    /// @param what
    void operator()(_Iterator first_iter, _Iterator input_last_iter, _Iterator error_iter,
                    boost::spirit::info what) const {
        auto last_pos1 = getLastIteratorPosition(first_iter, error_iter);
        auto last_pos2 = last_pos1;
        last_pos2.column = last_pos1.column + 1;

        std::stringstream ss;
        ss << what;

        std::string rule_name = what.tag == "token_def" ? ss.str() : "\"" + what.tag + "\"";
        logger->error(fmt::format("expect a {}", rule_name), last_pos1, last_pos2, BLUB);
    }

   private:
    std::shared_ptr<Logger> logger;
};

template <typename _Iterator>
struct SuccessHandler {
    SuccessHandler(std::shared_ptr<Logger> logger) { this->logger = logger; }

    template <typename _Ast>
    void operator()(_Iterator first_iter, _Iterator input_last_iter, _Iterator last_iter,
                    _Ast& attribute) const {
        attribute.first_position = getFirstIteratorPosition(first_iter);
        attribute.last_position = getLastIteratorPosition(first_iter, last_iter);
    }

    void operator()(_Iterator first_iter, _Iterator input_last_iter, _Iterator last_iter,
                    ast::Annotations& attribute) const {
        attribute.first_position = getFirstIteratorPosition(first_iter);
        attribute.last_position = getLastIteratorPosition(first_iter, last_iter);
    }

    void operator()(_Iterator first_iter, _Iterator input_last_iter, _Iterator last_iter,
                    ast::Operand& ast_operand) const {
        boost::apply_visitor([=](auto& x) { (*this)(first_iter, input_last_iter, last_iter, x); },
                             ast_operand);
    }

    void operator()(_Iterator first_iter, _Iterator input_last_iter, _Iterator last_iter,
                    ast::TemplateArgument& ast_template_argument) const {
        boost::apply_visitor([=](auto& x) { (*this)(first_iter, input_last_iter, last_iter, x); },
                             ast_template_argument);
    }

    void operator()(_Iterator first_iter, _Iterator input_last_iter, _Iterator last_iter,
                    ast::Statement& ast_statement) const {
        boost::apply_visitor([=](auto& x) { (*this)(first_iter, input_last_iter, last_iter, x); },
                             ast_statement);
    }

    void operator()(_Iterator first_iter, _Iterator input_last_iter, _Iterator last_iter,
                    ast::PostfixTypeOperator& ast_postfix_type_operator) const {
        boost::apply_visitor([=](auto& x) { (*this)(first_iter, input_last_iter, last_iter, x); },
                             ast_postfix_type_operator);
    }

    template <typename _T>
    void operator()(_Iterator first_iter, _Iterator input_last_iter, _Iterator last_iter,
                    boost::optional<_T>& ast_optional) const {
        if (ast_optional) {
            (*this)(first_iter, input_last_iter, last_iter, *ast_optional);
        }
    }

    void operator()(_Iterator first_iter, _Iterator input_last_iter, _Iterator last_iter,
                    ast::Statements& attribute) const {
        CodeLexerType tokens;
        auto skip = boost::spirit::qi::in_state("WS")[tokens.self];
        using boost::spirit::qi::unused;
        // boost::spirit::qi::skip_over(last_iter, input_last_iter, skipper);
        // skipper.parse(last_iter, input_last_iter, unused, unused, unused);

        // typedef typename result_of::compile<qi::domain, Skipper>::type skipper_type;
        auto skipper = boost::spirit::qi::compile<boost::spirit::qi::domain>(skip);
        boost::spirit::qi::skip_over(last_iter, input_last_iter, skipper);

        if (last_iter != input_last_iter) {
            auto pos1 = getFirstIteratorPosition(last_iter);
            auto pos2 = pos1;
            pos2.column = pos1.column + 1;
            logger->error("expect a \"statement\"", pos1, pos2, REDB);
        }
    }

   private:
    std::shared_ptr<Logger> logger;
};

}  // namespace prajna::parser::grammar
