/*

Copyright (c) 2014 Jarryd Beck

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/
#include <fstream>
#include <iostream>
#include <memory>

#include "boost/asio/io_service.hpp"
#include "boost/process/io.hpp"
#include "boost/process/search_path.hpp"
#include "boost/process/system.hpp"
#include "cxxopts.hpp"
#include "fmt/color.h"
#include "fmt/format.h"
#include "nlohmann/json.hpp"
#include "prajna/compiler/compiler.h"
#include "repl/repl.h"

int prajna_exe_main(int argc, char* argv[]) {
    cxxopts::Options options("prajna exe");
    options.allow_unrecognised_options().positional_help("program").custom_help("[options]");
    options.add_options()("h,help", "prajna exe help")("program", "program file",
                                                       cxxopts::value<std::string>());
    options.parse_positional({"program"});
    auto result = options.parse(argc, argv);

    if (result.count("program")) {
        auto compiler = prajna::Compiler::create();
        auto program_path = std::filesystem::path(result["program"].as<std::string>());
        compiler->compileBuiltinSourceFiles("prajna_builtin_packages");
        compiler->addPackageDirectoryPath(std::filesystem::current_path());
        compiler->executeProgram(program_path);
        return 0;
    }

    fmt::print(options.help({""}));
    return 0;

    return 0;
}

int prajna_jupyter_main(int argc, char* argv[]) {
    cxxopts::Options options("prajna jupyter");
    options.allow_unrecognised_options().custom_help("[options]");
    options.add_options()("h,help", "prajna jupyter help")("i, install",
                                                           "install jupyter prajna kernel");
    auto result = options.parse(argc, argv);

    auto executable_path = std::filesystem::path(argv[0]);
    auto xeus_path =
        std::filesystem::current_path() / executable_path.parent_path() / "xeus_prajna";
    if (!std::filesystem::is_regular_file(xeus_path)) {
        fmt::print(
            "xeus_prajna is not found, please configure cmake with -DPRAJNA_WITH_JUPYTER=ON");
    }

    if (result.count("install")) {
        nlohmann::json prajna_kernel_json = {
            {"display_name", "Prajna"},
            {"argv", {xeus_path.lexically_normal().string(), "-f", "{connection_file}"}},
            {"language", "prajna"}};

        boost::asio::io_service ios;
        std::future<std::string> future_jupyter_data_dir;
        auto jupyter_path = boost::process::search_path("jupyter");
        if (jupyter_path.empty()) {
            fmt::print("please install jupyter first");
        }
        boost::process::system(fmt::format("{} --data-dir", jupyter_path.string()),
                               boost::process::std_out > future_jupyter_data_dir);
        auto jupyter_data_dir = future_jupyter_data_dir.get();
        // 末尾存在换行符, 将其删除
        jupyter_data_dir.pop_back();
        std::ofstream kernel_json_ofs(jupyter_data_dir + "/kernels/prajna/kernel.json");
        if (!kernel_json_ofs.good()) {
            fmt::print("failed to create jupyter prajna kernel.jsonf file: {}\n",
                       jupyter_data_dir + "/kernels/prajna/kernel.json");
        }
        kernel_json_ofs << prajna_kernel_json;
        fmt::print("success to install jupyter prajna kernel in {}\n",
                   jupyter_data_dir + "/kernels/prajna");

        return 0;
    }

    fmt::print(options.help({""}));
    return 0;
}

int main(int argc, char* argv[]) {
    cxxopts::Options options("prajna");
    options.custom_help("[options]").allow_unrecognised_options().positional_help("subcommand");
    options.add_options()("subcommand", "execute a prajna program", cxxopts::value<std::string>())(
        "h,help", "prajna help");
    options.parse_positional({"subcommand"});
    auto result = options.parse(argc, argv);

    if (result.count("subcommand")) {
        auto sub_command = result["subcommand"].as<std::string>();

        std::vector<char*> sub_argv(argv, argv + argc);
        sub_argv.erase(std::next(sub_argv.begin()));
        auto sub_argc = argc - 1;

        if (sub_command == "exe") {
            return prajna_exe_main(sub_argc, sub_argv.data());
        }

        if (sub_command == "repl") {
            cxxopts::Options options("prajna repl");
            options.custom_help("[options]")
                .allow_unrecognised_options()
                .positional_help("subcommand");
            options.add_options()("h,help", "prajna repl help");
            auto result = options.parse(sub_argc, sub_argv.data());

            return prajna_repl_main();
        }

        if (sub_command == "jupyter") {
            return prajna_jupyter_main(sub_argc, sub_argv.data());
        }

        fmt::print("unsupported sub command {}\n",
                   fmt::styled(sub_command, fmt::fg(fmt::color::red)));
    }

    if (result.count("help") || result.arguments().empty()) {
        fmt::print(options.help({""}));
        fmt::print("Avaliabled sub commands: exe, repl, jupyter\n");
        fmt::print("Sub command usage:\n prajna exe --help\n");
        return 0;
    }

    return 0;
}
