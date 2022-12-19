#pragma once

#include <iostream>

#include "boost/spirit/include/lex_lexertl.hpp"
#include "boost/spirit/include/lex_lexertl_position_token.hpp"
#include "prajna/ast/ast.hpp"

namespace prajna::parser {

namespace lex = boost::spirit::lex;

template <typename Lexer>
struct CodeLexer : lex::lexer<Lexer> {
    CodeLexer();

    lex::token_def<lex::omit> arrow;         // ->
    lex::token_def<ast::Operator> scope;     // ::
    lex::token_def<ast::Operator> period;    // .
    lex::token_def<ast::Operator> backslash; /* \ */

    lex::token_def<lex::omit> left_angle_brackets;   // <<<
    lex::token_def<lex::omit> right_angle_brackets;  // >>>

    lex::token_def<ast::Operator> or_, and_, xor_, not_;  // logical

    lex::token_def<ast::Operator> equal, not_equal, less, less_or_equal, greater,
        greater_or_equal;  // compare

    lex::token_def<ast::Operator> plus, minus, times, divide, remain;  // arithmetic

    lex::token_def<ast::Operator> shift_left, shift_right;

    lex::token_def<ast::SourceLocation> assign;

    lex::token_def<ast::Operator> l_bracket;    // left bracket
    lex::token_def<lex::omit> r_bracket;        // right bracket
    lex::token_def<ast::Operator> l_s_bracket;  // left square bracket
    lex::token_def<lex::omit> r_s_bracket;      // right square bracket
    lex::token_def<lex::omit> l_braces;         // left braces;
    lex::token_def<lex::omit> r_braces;         // right braces;
    lex::token_def<lex::omit> l_a_bracket;      // left angle bracket <;
    lex::token_def<lex::omit> r_a_bracket;      // right angle bracket >;

    lex::token_def<lex::omit> comma;      //,
    lex::token_def<lex::omit> colon;      //:
    lex::token_def<lex::omit> semicolon;  //;

    lex::token_def<lex::omit> at;           // @
    lex::token_def<lex::omit> number_sign;  // #

    lex::token_def<lex::omit> func;
    lex::token_def<lex::omit> struct_;
    lex::token_def<lex::omit> implement;
    lex::token_def<lex::omit> interface;
    lex::token_def<lex::omit> template_;

    lex::token_def<lex::omit> import, as, export_;

    lex::token_def<lex::omit> if_, else_, while_, for_, in, to;

    lex::token_def<lex::omit> return_;
    lex::token_def<ast::Break> break_;
    lex::token_def<ast::Continue> continue_;

    lex::token_def<lex::omit> var;
    lex::token_def<ast::Identifier> this_;
    lex::token_def<lex::omit> cast;
    lex::token_def<lex::omit> sizeof_;

    lex::token_def<ast::Identifier> identifier;

    lex::token_def<ast::CharLiteral> char_literal;
    lex::token_def<ast::StringLiteral> string_literal;
    lex::token_def<ast::IntLiteral> int_literal;
    lex::token_def<ast::FloatLiteral> float_literal;
    lex::token_def<ast::BoolLiteral> true_literal, false_literal;

    lex::token_def<lex::omit> newline;
    lex::token_def<lex::omit> space;
    lex::token_def<lex::omit> c_comment;
    lex::token_def<lex::omit> line_comment;
};

}  // namespace prajna::parser
