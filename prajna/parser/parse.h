#pragma once

#include <memory>
#include <string>

#include "prajna/ast/ast.hpp"

namespace prajna {
class Logger;
}

namespace prajna::parser {

bool parse(std::string code, prajna::ast::Statements& ast, std::string file_name,
           std::shared_ptr<Logger> logger);

std::shared_ptr<prajna::ast::Statements> parse(std::string code, std::string file_name,
                                               std::shared_ptr<Logger> logger);

}  // namespace prajna::parser
