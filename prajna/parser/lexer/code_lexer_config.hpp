#pragma once

#include "boost/spirit/home/classic/iterator/position_iterator.hpp"
#include "boost/spirit/home/support/multi_pass.hpp"
#include "prajna/parser/lexer/code_lexer.h"

namespace prajna::parser {
namespace lex = boost::spirit::lex;
namespace spirit = boost::spirit;

// @brief 输入到词法解析里的迭代器类型, 其会将string包裹上位置信息
typedef spirit::classic::position_iterator2<std::string::iterator> base_iterator_type;

// @brief 词法解析器的token类型
typedef lex::lexertl::position_token<
    base_iterator_type, boost::mpl::vector<prajna::ast::AstBase, prajna::ast::CharLiteral,
                                           ast::StringLiteral, ast::IntLiteral, ast::FloatLiteral,
                                           ast::BoolLiteral, ast::Operator, ast::Identifier>>
    TokenType;

// @brief 应该是词法解析器的引擎
typedef lex::lexertl::lexer<TokenType> LexertlLexer;
// @brief 词法解析的具体类型
typedef prajna::parser::CodeLexer<LexertlLexer> CodeLexerType;
typedef CodeLexerType::iterator_type iterator_type;
}  // namespace prajna::parser
