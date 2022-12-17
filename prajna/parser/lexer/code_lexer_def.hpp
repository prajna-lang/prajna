#include "prajna/parser/lexer/code_lexer.h"

namespace prajna::parser {

template <typename Lexer>
CodeLexer<Lexer>::CodeLexer() {
    // newline = "[\\r\\n]";
    space = "\\s";
    c_comment = "\\/\\*[^*]*\\*+([^/*][^*]*\\*+)*\\/";
    line_comment = "\\/\\/[^\\n]*\\n";
    this->self("WS") = space | c_comment | line_comment;

    // 需要在前面否在会干扰其他一个字符的运算符
    shift_left = "<<";
    shift_right = ">>";
    this->self = shift_left | shift_right;

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

    left_angle_brackets = "<<<";
    right_angle_brackets = ">>>";
    this->self += left_angle_brackets | right_angle_brackets;

    and_ = "&|and";
    or_ = "\\||or";
    not_ = "!|not";
    xor_ = "\\^|xor";
    this->self += and_ | or_ | xor_ | not_;

    plus = '+';
    minus = '-';
    times = '*';
    divide = '/';
    remain = '%';
    this->self += plus | minus | times | divide | remain;

    assign = '=';
    this->self += assign;

    // signs
    l_bracket = '(';
    r_bracket = ')';
    l_s_bracket = '[';
    r_s_bracket = ']';
    l_braces = '{';
    r_braces = '}';
    l_a_bracket = '<';
    r_a_bracket = '>';
    this->self += l_bracket | r_bracket | l_s_bracket | r_s_bracket | l_braces | r_braces |
                  l_a_bracket | r_a_bracket;

    comma = ',';
    colon = ':';
    semicolon = ';';
    this->self += comma | colon | semicolon;

    at = '@';
    this->self += at;

    func = "func";
    struct_ = "struct";
    implement = "implement";
    interface = "interface";
    template_ = "template";
    import = "import";
    export_ = "export";
    as = "as";
    this->self += func | struct_ | implement | interface | template_ | import | as | export_;

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
    cast = "cast";
    sizeof_ = "sizeof";
    this->self += var | this_ | cast | sizeof_;

    char_literal =
        "('[^'\\\"\\\\]')|('\\\\a')|('\\\\b')|('\\\\f')|('\\\\n')|('\\\\r')|('\\\\t')|('"
        "\\\\v')|('"
        "\\\\\\\\')|('\\\\'')|('\\\\\\\"')|('\\\\0')";
    this->self += char_literal;

    string_literal = "\\\"([^\"]|\\\\\\\")*\\\"";
    this->self += string_literal;

    int_literal = "(([1-9][0-9]*)|0)";
    float_literal = "(([1-9][0-9]*)|0)\\.[0-9]*";
    this->self += int_literal | float_literal;

    true_literal = "true";
    false_literal = "false";
    this->self += true_literal | false_literal;

    identifier = "[a-zA-Z_][a-zA-Z0-9_]*";
    this->self += identifier;
}

}  // namespace prajna::parser
