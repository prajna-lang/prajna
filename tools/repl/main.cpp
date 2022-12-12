#include <cstdlib>
#include <iostream>
#include <string>

#include "fmt/color.h"
#include "fmt/format.h"
#include "prajna/compiler/compiler.h"

int main(int argc, char* argv[]) {
    auto compiler = std::make_shared<prajna::compiler::Compiler>();
    compiler->compileBuiltinSourceFiles("prajna/builtin_sources");

    while (true) {
        std::string code_line = "";
        std::cin.clear();
        std::cout << "\n>> ";
        std::getline(std::cin, code_line);
        if (code_line.back() != ';' || code_line.back() != '}') {
            code_line.push_back(';');
        }
        compiler->compileCommandLine(code_line);
    }

    return 0;
}
