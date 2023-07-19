#include "repl/repl.h"

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <stdexcept>
#include <string>
#include <vector>

#include "cpp-terminal/input.hpp"
#include "cpp-terminal/prompt.hpp"
#include "prajna/compiler/compiler.h"
#include "prajna/helper.hpp"

using Term::Key;
using Term::prompt_multiline;
using Term::Terminal;

bool determine_completeness(CPP_TERMINAL_MAYBE_UNUSED std::string command) {
    // Determine if the statement is complete
    bool complete;
    if (command.size() > 1 && command.substr(command.size() - 2, 1) == "\\") {
        complete = false;
    } else {
        complete = true;
    }
    return complete;
}

std::string __input;

int prajna_repl_main(int argc, char* argv[]) {
    prajna::input_callback = [&]() -> char* {
        Term::terminal.store_and_restore();

        __input.clear();
        while (true) {
            auto c = getchar();
            if (c == EOF || c == 10 || c == 4) {
                break;
            }
            __input.push_back(static_cast<char>(c));
        }

        Term::terminal.store_and_restore();
        Term::terminal.setOptions({Term::Option::NoClearScreen, Term::Option::SignalKeys,
                                   Term::Option::Cursor, Term::Option::Raw});
        return (char*)__input.c_str();
    };

    Term::terminal << "Prajna 0.0.0, all copyrights @ \"www.github.com/matazure\"" << std::endl;

    auto compiler = prajna::Compiler::Create();
    if (std::filesystem::exists("prajna_builtin_packages")) {
        compiler->CompileBuiltinSourceFiles("prajna_builtin_packages");
    } else {
        auto prajna_builtin_packages_directory =
            prajna::ProgramLocation(argv[0]).parent_path() / "../prajna_builtin_packages";
        compiler->CompileBuiltinSourceFiles(prajna_builtin_packages_directory.string());
    }

    Term::terminal.setOptions({Term::Option::NoClearScreen, Term::Option::SignalKeys,
                               Term::Option::Cursor, Term::Option::Raw});
    std::vector<std::string> history;
    while (true) {
        std::function<bool(std::string)> iscomplete = [](std::string code_line) {
            return code_line.empty() || code_line[code_line.size() - 2] != '\\';
        };
        std::string code_line = Term::prompt_multiline("prajna > ", history, iscomplete);

        if (code_line.size() == 1 && code_line[0] == Term::Key::CTRL_C) break;
        if (code_line == "quit") break;

        // 移除换行符号 '\\'
        std::list<char> code_line_list(RANGE(code_line));
        for (auto iter = code_line_list.begin(); iter != std::prev(code_line_list.end());) {
            if (*iter == '\\') {
                if ((*std::next(iter)) == '\n' || (*std::next(iter)) == '\r') {
                    iter = code_line_list.erase(iter);
                    continue;
                }
            }

            ++iter;
        }
        code_line = std::string(RANGE(code_line_list));
        // ;可以重复, 不会导致错误. 插入到\n前面, 这样错误信息才正确
        auto last_char = *std::prev(code_line.end(), 2);
        if (code_line.size() >= 2 && !(last_char == ';' || last_char == '}')) {
            code_line.insert(std::prev(code_line.end()), ';');
        }
        // code_line.push_back('\n'); // 自带\n
        compiler->ExecuteCodeInRelp(code_line);

        std::cout << "\n";
    }

    return 0;
}
