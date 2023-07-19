

#include <chrono>
#include <filesystem>
#include <fstream>

#include "fmt/printf.h"
#include "gtest/gtest.h"
#include "prajna/compiler/compiler.h"
#include "prajna/exception.hpp"
#include "tests/utility.hpp"

using namespace prajna;

struct PrintFileName {
    template <class ParamType>
    std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
        return std::filesystem::path(info.param).stem().string();
    }
};

class CompilerSourceTests : public testing::TestWithParam<std::string> {};
TEST_P(CompilerSourceTests, TestSourceFromDirectory) {
    auto compiler = Compiler::Create();
    std::string prajna_source_path = GetParam();
    compiler->AddPackageDirectoryPath(".");
    auto t0 = std::chrono::high_resolution_clock::now();
    compiler->CompileBuiltinSourceFiles("prajna_builtin_packages");
    auto t1 = std::chrono::high_resolution_clock::now();
    fmt::print("compiling cost time: {}ms\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    compiler->RunTests(prajna_source_path);
    auto t2 = std::chrono::high_resolution_clock::now();
    fmt::print("execution cost time: {}ms\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
}

// 会遍历整个文件夹里的文件
INSTANTIATE_TEST_SUITE_P(
    CompilerSourceTestsInstance, CompilerSourceTests,
    testing::ValuesIn(prajna::tests::getFiles("tests/compiler/prajna_sources")), PrintFileName());

#ifdef PRAJNA_WITH_GPU

// 会遍历整个文件夹里的文件
INSTANTIATE_TEST_SUITE_P(
    CompilerSourceTestsGpuInstance, CompilerSourceTests,
    testing::ValuesIn(prajna::tests::getFiles("tests/compiler/prajna_gpu_sources")),
    PrintFileName());

#endif
