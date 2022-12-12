#include "prajna/parser/grammar/statement_grammar_def.hpp"
#include "prajna/parser/lexer/code_lexer_config.hpp"

namespace prajna::parser::grammar {

template struct StatementGrammer<iterator_type, CodeLexerType>;

}  // namespace prajna::parser::grammar
