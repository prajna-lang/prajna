

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

#include "fmt/printf.h"
#include "gtest/gtest.h"
#include "prajna/compiler/compiler.h"
#include "prajna/exception.hpp"

using namespace prajna;

inline std::vector<std::string> getFiles(std::string dir) {
    std::vector<std::string> files;
    for (const auto& file : std::filesystem::recursive_directory_iterator(dir)) {
        if (std::filesystem::is_directory(file)) continue;
        files.push_back(file.path().string());
    }

    // 排一下顺序, 这样就能直接根据文件顺序来选择测试
    std::sort(files.begin(), files.end());

    return files;
}

struct PrintFileName {
    template <class ParamType>
    std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
        auto test_file_path = std::filesystem::path(info.param).replace_extension("");
        std::string name;
        for (auto iter = std::next(test_file_path.begin(), 2); iter != test_file_path.end();
             ++iter) {
            name += iter->string();
            if (std::next(iter) == test_file_path.end()) continue;
            name += "_s_";
        }
        return name;
    }
};

class PrajnaTests : public testing::TestWithParam<std::string> {};
TEST_P(PrajnaTests, TestSourceFile) {
    auto compiler = Compiler::Create();
    std::string prajna_source_path = GetParam();
    compiler->AddPackageDirectoryPath(".");
    auto t0 = std::chrono::high_resolution_clock::now();
    compiler->CompileBuiltinSourceFiles("builtin_packages");
    auto t1 = std::chrono::high_resolution_clock::now();
    // fmt::print("compiling cost time: {}ms\n",
    //            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    std::cout << std::format("compiling cost time: {}ms\n",
                             std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    compiler->RunTests(prajna_source_path);
    auto t2 = std::chrono::high_resolution_clock::now();
    // fmt::print("execution cost time: {}ms\n",
    //            std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
    std::cout << std::format("execution cost time: {}ms\n",
                             std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
}

// 会遍历整个文件夹里的文件
INSTANTIATE_TEST_SUITE_P(PrajnaTestsInstance, PrajnaTests,
                         testing::ValuesIn(getFiles("tests/prajna_sources")), PrintFileName());
