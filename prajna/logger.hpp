#pragma once

#include <boost/algorithm/string.hpp>
#include <unordered_map>

#include "fmt/color.h"
#include "fmt/format.h"
#include "prajna/assert.hpp"
#include "prajna/ast/ast.hpp"
#include "prajna/compiler/compiler.h"
#include "prajna/exception.hpp"
#include "prajna/rich_bash.hpp"

namespace prajna {

enum struct LogLevel {
    info = 1,
    warning,
    error,
};
}

namespace fmt {

// @ref https://fmt.dev/latest/api.html#formatting-user-defined-types
template <>
struct formatter<prajna::LogLevel> {
    constexpr auto Parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto Format(const prajna::LogLevel& level, FormatContext& ctx) const -> decltype(ctx.out()) {
        // ctx.out() is an output iterator to write to.
        switch (level) {
            case prajna::LogLevel::info:
                return fmt::format_to(ctx.out(), "{}",
                                      fmt::styled("info", fmt::fg(fmt::color::gray)));
            case prajna::LogLevel::warning:
                return fmt::format_to(ctx.out(), "{}",
                                      fmt::styled("warning", fmt::fg(fmt::color::orange)));
            case prajna::LogLevel::error:
                return fmt::format_to(ctx.out(), "{}",
                                      fmt::styled("error", fmt::fg(fmt::color::red)));
        }

        return ctx.out();
    }
};

}  // namespace fmt

namespace prajna {

class Logger {
   protected:
    Logger() = default;

   public:
    static std::shared_ptr<Logger> Create(std::string code) {
        std::shared_ptr<Logger> self(new Logger);
        boost::split(self->_code_lines, code, boost::is_any_of("\n"));
        return self;
    }

    void Log(std::string message, ast::SourcePosition first_position,
             ast::SourcePosition last_position, std::string prompt, std::string locator_ascii_color,
             bool throw_error) {
        std::string what_message;
        if (first_position.file.empty()) {
            what_message = fmt::format("{}:{}: {}: {}\n", first_position.line,
                                       first_position.column, prompt, message);
        } else {
            what_message = fmt::format("{}:{}:{}: {}: {}\n", first_position.file,
                                       first_position.line, first_position.column, prompt, message);
        }

        print_callback(what_message);

        // 没有位置信息的话提前放回
        if (first_position.line < 0 || last_position.line < 0) {
            if (throw_error) {
                throw CompileError();
            }
            return;
        }

        std::vector<std::string> code_lines;
        for (int64_t i = first_position.line; i <= last_position.line; ++i) {
            code_lines.push_back(_code_lines[i - 1]);
        }

        if (last_position.column - 1 >
            code_lines[last_position.line - first_position.line].size()) {
            std::string spaces(last_position.column - 1 -
                                   code_lines[last_position.line - first_position.line].size(),
                               ' ');
            code_lines[last_position.line - first_position.line].append(spaces);
        }
        code_lines[last_position.line - first_position.line].insert(last_position.column - 1,
                                                                    std::string(RESET));
        // 不存在越界的情况
        PRAJNA_ASSERT(first_position.column - 1 >= 0);
        code_lines[0].insert(first_position.column - 1, locator_ascii_color);
        std::string code_region;
        for (auto code_line : code_lines) {
            code_region.append(code_line);
            code_region.append("\n");
        }

        print_callback(code_region);

        if (throw_error) {
            throw CompileError();
        }
    }

    void Error(std::string message, ast::SourcePosition first_position,
               ast::SourcePosition last_position, std::string locator_ascii_color) {
        auto error_prompt = fmt::format("{}", fmt::styled("error", fmt::fg(fmt::color::red)));
        this->Log(message, first_position, last_position, error_prompt, locator_ascii_color, true);
    }

    void Error(std::string message) {
        fmt::print("{}: {}\n", fmt::styled("error", fmt::fg(fmt::color::red)), message);
        throw CompileError();
    }

    void Error(std::string message, ast::SourcePosition first_position) {
        auto last_position = first_position;
        last_position.column = first_position.column + 1;
        Error(message, first_position, last_position, std::string(BLU));
    }

    void Error(std::string message, ast::SourceLocation source_location) {
        Error(message, source_location.first_position, source_location.last_position,
              std::string(BLU));
    }

    void Error(std::string message, ast::Operand ast_operand) {
        boost::apply_visitor([=](auto x) { this->Error(message, x); }, ast_operand);
    }

    void Warning(std::string message, ast::SourceLocation source_location) {
        auto warning_prompt =
            fmt::format("{}", fmt::styled("warning", fmt::fg(fmt::color::orange)));
        this->Log(message, source_location.first_position, source_location.last_position,
                  warning_prompt, std::string(BLU), false);
    }

    void Warning(std::string message, ast::Operand ast_operand) {
        boost::apply_visitor([=](auto x) { this->Warning(message, x); }, ast_operand);
    }

    void Note(ast::SourceLocation source_location) {
        auto warning_prompt = fmt::format("{}", fmt::styled("note", fmt::fg(fmt::color::gray)));
        this->Log("", source_location.first_position, source_location.last_position, warning_prompt,
                  std::string(YEL), false);
    }

   private:
    std::vector<std::string> _code_lines;
};

}  // namespace prajna
