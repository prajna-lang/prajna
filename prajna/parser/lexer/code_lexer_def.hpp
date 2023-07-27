#include "prajna/parser/lexer/code_lexer.h"

namespace prajna::parser {

template <typename Lexer>
CodeLexer<Lexer>::CodeLexer() {
    // newline = "[\\r\\n]";
    space = "\\s";
    c_comment = "\\/\\*[^*]*\\*+([^/*][^*]*\\*+)*\\/";
    line_comment = "\\/\\/[^\\n]*\\n";
    this->self("WS") = space | c_comment | line_comment;

    left_arrow2 = "<\\|";
    right_arrow2 = "\\|>";
    this->self += left_arrow2 | right_arrow2;

    equal = "==";
    not_equal = "!=";
    less_or_equal = "<=";
    greater_or_equal = ">=";
    less = "<";
    greater = ">";
    this->self += equal | not_equal | less_or_equal | greater_or_equal | less | greater;

    arrow = "->";
    scope = "::";
    period = '.';
    this->self += arrow | scope | period;

    and_ = "&";
    or_ = "\\|";
    not_ = "!";
    xor_ = "\\^";
    this->self += and_ | or_ | xor_ | not_;

    plus = '+';
    minus = '-';
    star = '*';
    divide = '/';
    remain = '%';
    this->self += plus | minus | star | divide | remain;

    assign = '=';
    this->self += assign;

    // signs
    left_bracket = '(';
    right_bracket = ')';
    left_square_bracket = '[';
    right_square_bracket = ']';
    left_braces = '{';
    r_braces = '}';
    l_a_bracket = '<';
    r_a_bracket = '>';
    this->self += left_bracket | right_bracket | left_square_bracket | right_square_bracket |
                  left_braces | r_braces | l_a_bracket | r_a_bracket;

    comma = ',';
    colon = ':';
    semicolon = ';';
    this->self += comma | colon | semicolon;

    at = '@';
    number_sign = '#';
    this->self += at | number_sign;

    module_ = "module";
    func = "func";
    struct_ = "struct";
    implement = "implement";
    interface = "interface";
    template_ = "template";
    use = "use";
    as = "as";
    this->self += module_ | func | struct_ | implement | interface | template_ | use | as;

    if_ = "if";
    else_ = "else";
    while_ = "while";
    for_ = "for";
    in = "in";
    to = "to";
    return_ = "return";
    break_ = "break";
    continue_ = "continue";
    this->self += if_ | else_ | for_ | in | to | while_ | return_ | break_ | continue_;

    var = "var";
    this_ = "this";
    this->self += var | this_;

    char_literal =
        "('[^'\\\"\\\\]')|('\\\\a')|('\\\\b')|('\\\\f')|('\\\\n')|('\\\\r')|('\\\\t')|('"
        "\\\\v')|('"
        "\\\\\\\\')|('\\\\'')|('\\\\\\\"')|('\\\\0')";
    this->self += char_literal;

    string_literal = "\\\"([^\"]|\\\\\\\")*\\\"";
    this->self += string_literal;

    int_literal = "[0-9]+";
    float_literal = "[0-9]*\\.?[0-9]+(e[-+]?[0-9]+)?";

    this->self += int_literal | float_literal;

    true_literal = "true";
    false_literal = "false";
    this->self += true_literal | false_literal;

    identifier = "[a-zA-Z_][a-zA-Z0-9_]*";
    this->self += identifier;
}

}  // namespace prajna::parser
