#pragma once

#include <exception>
#include <string>

#include "prajna/config.hpp"

namespace prajna {

class assert_failed : public std::exception {
   public:
    assert_failed(const std::string& expr, const std::string& function, const std::string& file,
                  int64_t line, bool verify, const std::string& msg)
        : _expr(expr), _function(function), _file(file), _line(line), _msg(msg) {
        std::string with_msg;
        if (!msg.empty()) {
            with_msg = ", with message \"" + msg + "\"";
        }
        std::string assert_or_verify = verify ? "verify" : "assert";

        _what_str = "failed to " + assert_or_verify + " \"" + expr + "\"" + " in " + _file + ":" +
                    std::to_string(_line) + with_msg;
    }

    virtual const char* what() const noexcept override { return _what_str.c_str(); }

   private:
    std::string _expr;
    std::string _function;
    std::string _file;
    int64_t _line;
    std::string _msg;
    std::string _what_str;
};

inline void raise_assert_failed(const std::string& expr, const std::string& function,
                                const std::string& file, long line, bool verify = false,
                                const std::string& msg = "") {
    throw assert_failed(expr, function, file, line, verify, msg);
}

}  // namespace prajna

#if defined(PRAJNA_DISABLE_ASSERT)
#define PRAJNA_ASSERT(expr, ...) ((void)0)
#else
#define PRAJNA_ASSERT(expr, ...)                                                            \
    ((!!(expr)) ? ((void)0)                                                                 \
                : ::prajna::raise_assert_failed(#expr, __func__, __FILE__, __LINE__, false, \
                                                ##__VA_ARGS__))
#endif

#define PRAJNA_VERIFY(expr, ...)                                                           \
    ((!!(expr)) ? ((void)0)                                                                \
                : ::prajna::raise_assert_failed(#expr, __func__, __FILE__, __LINE__, true, \
                                                ##__VA_ARGS__))

#define PRAJNA_UNREACHABLE PRAJNA_VERIFY(false, "unreachable")
#define PRAJNA_UNIMPLEMENT PRAJNA_VERIFY(false, "unimplement")
#define PRAJNA_TODO PRAJNA_VERIFY(false, "todo")
