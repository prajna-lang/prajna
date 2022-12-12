#pragma once

#include <boost/algorithm/string.hpp>
#include <map>

#include "fmt/color.h"
#include "fmt/format.h"
#include "prajna/assert.hpp"
#include "prajna/ast/ast.hpp"

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
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') throw format_error("invalid format");
        return it;
    }

    template <typename FormatContext>
    auto format(const prajna::LogLevel& level, FormatContext& ctx) const -> decltype(ctx.out()) {
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

enum Language { EN = 1, CH };

class Logger {
   protected:
    Logger() = default;

   public:
    static std::shared_ptr<Logger> create(std::string code, Language language) {
        std::shared_ptr<Logger> self(new Logger);
        boost::split(self->_code_lines, code, boost::is_any_of("\n"));
        self->_language = language;
        return self;
    }

    void log(LogLevel level, std::map<Language, std::string> messages,
             ast::SourcePosition position) {
        PRAJNA_ASSERT(messages.count(_language), "对应语言的信息没有输入");

        fmt::print("{}", fmt::underlying(fmt::color::red), "0fdsafsda0");
        auto message = messages[_language];
        fmt::print("{}:{}:{}: {}: {}\n", position.file, position.line, position.column, level,
                   message);
        // 行数和列数是从1开始计数的, 所以减一
        PRAJNA_ASSERT(position.line - 1 > 0 && position.line - 1 < _code_lines.size());
        auto code_line = _code_lines[position.line - 1];
        //符号不对
        PRAJNA_ASSERT(position.column - 1 > 0 && position.column - 1 < code_line.size());
        code_line.insert(code_line.begin() + position.column - 1, '^');
        fmt::print("{}\n", fmt::styled(code_line, fmt::bg(fmt::color::blue)));
    }

   private:
    Language _language;
    std::vector<std::string> _code_lines;
};

}  // namespace prajna
