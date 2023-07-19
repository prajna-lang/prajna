#pragma once

#include <filesystem>

#include "prajna/compiler/compiler.h"
#include "xeus/xinterpreter.hpp"
#include "xwidgets/xslider.hpp"

namespace xeus_prajna {

class PrajnaXeusInterpreter : public xeus::xinterpreter {
   public:
    PrajnaXeusInterpreter(std::string builtin_packages_directory) {
        xeus::register_interpreter(this);
        compiler = prajna::Compiler::Create();
        compiler->CompileBuiltinSourceFiles(builtin_packages_directory);
    }

    virtual ~PrajnaXeusInterpreter() = default;

   private:
    void configure_impl() override {}

    nl::json execute_request_impl(int execution_counter, const std::string& code, bool silent,
                                  bool store_history, nl::json user_expressions,
                                  bool allow_stdin) override {
        prajna::print_callback = [=, compiler = compiler](std::string str) {
            nl::json pub_data;
            pub_data["text/plain"] = str;
            publish_execution_result(execution_counter, pub_data, nl::json::object());
        };

        std::string value;
        prajna::input_callback = [=, &value]() -> char* {
            this->register_input_handler([&value](const std::string& v) { value = v; });
            std::string prompt = "input";
            bool password = false;
            this->input_request("", password);
            return (char*)value.c_str();
        };

        auto code_line = code;
        if (code_line.size() >= 1 && !(code_line.back() == ';' || code_line.back() == '}')) {
            code_line.push_back(';');
        }
        code_line.push_back('\n');
        compiler->ExecuteCodeInRelp(code_line);

        // auto slider = new xw::slider<double>;
        // slider->display();

        nl::json result;
        result["status"] = "ok";
        result["payload"] = nl::json::array();
        result["user_expressions"] = nl::json::object();
        return result;
    }

    nl::json complete_request_impl(const std::string& code, int cursor_pos) override {
        nl::json result;
        result["status"] = "ok";
        return result;
    }

    nl::json inspect_request_impl(const std::string& code, int cursor_pos,
                                  int detail_level) override {
        nl::json result;
        result["status"] = "ok";
        return result;
    }

    /// @brief 并非必须的,故直接放回complete的状态
    nl::json is_complete_request_impl(const std::string& code) override {
        nl::json result;
        result["status"] = "complete";
        return result;
    }

    nl::json kernel_info_request_impl() override {
        nl::json result;
        const std::string PRAJNA_VERSION = "0.0.0";
        result["implementation"] = "xprajna";
        result["implementation_version"] = PRAJNA_VERSION;

        std::string banner =
            ""
            "  __  _____ _   _ ___\n"
            "  \\ \\/ / _ \\ | | / __|\n"
            "   >  <  __/ |_| \\__ \\\n"
            "  /_/\\_\\___|\\__,_|___/\n"
            "\n"
            "  xeus-prajna: a Jupyter Kernel for Prajna\n";
        result["banner"] = banner;
        result["language_info"]["name"] = "prajna";
        // result["language_info"]["codemirror_mode"] = "prajna";
        result["language_info"]["mimetype"] = "text/x-prajna";
        result["language_info"]["version"] = PRAJNA_VERSION;
        // 会决定download时的文件名后缀
        result["language_info"]["file_extension"] = ".prajnascripts";

        return result;
    }
    void shutdown_request_impl() override {}

   private:
    std::shared_ptr<prajna::Compiler> compiler;
};

}  // namespace xeus_prajna
