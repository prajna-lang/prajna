//
//  prajna_parser.h
//  binding
//
//  Created by 张志敏 on 11/13/13.
//
//

#pragma once

#include <memory>
#include <string>

#include "prajna/ast/ast.hpp"

namespace prajna {
class Logger;
}

namespace prajna::parser {

__attribute__((no_sanitize("address"))) bool parse(std::string code, prajna::ast::Statements& ast,
                                                   std::string file_name,
                                                   std::shared_ptr<Logger> logger);

std::shared_ptr<prajna::ast::Statements> parse(std::string code, std::string file_name,
                                               std::shared_ptr<Logger> logger);

}  // namespace prajna::parser
