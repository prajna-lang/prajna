

#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"
#include "prajna/compiler/compiler.h"
#include "tests/utility.hpp"

using namespace prajna;

class CompilerSourceTests : public testing::TestWithParam<std::string> {};

TEST_P(CompilerSourceTests, TestFromDirectory) {
    auto compiler = std::make_shared<compiler::Compiler>();
    std::string prajna_source_path = GetParam();
    compiler->compileBuiltinSourceFiles("prajna/builtin_sources");
    compiler->compileFile(".", prajna_source_path);
    compiler->runTestFunctions();
}

struct PrintFileName {
    template <class ParamType>
    std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
        return std::filesystem::path(info.param).stem();
    }
};

class CompilerCommandLineTests : public testing::TestWithParam<std::string> {};

TEST_P(CompilerCommandLineTests, TestFromDirectory) {
    auto compiler = std::make_shared<compiler::Compiler>();
    compiler->compileBuiltinSourceFiles("prajna/builtin_sources");

    std::string prajna_script_path = GetParam();

    std::ifstream ifs(prajna_script_path);
    while (ifs.good()) {
        std::string code;
        std::getline(ifs, code);
        compiler->compileCommandLine(code);
    }
}

INSTANTIATE_TEST_SUITE_P(
    CompilerCommandLineTestsInstance, CompilerCommandLineTests,
    testing::ValuesIn(prajna::tests::getFiles("tests/compiler/prajna_scripts")));

// 会遍历整个文件夹里的文件
INSTANTIATE_TEST_SUITE_P(
    CompilerSourceTestsInstance, CompilerSourceTests,
    testing::ValuesIn(prajna::tests::getFiles("tests/compiler/prajna_sources")), PrintFileName());

#ifdef PRAJNA_WITH_GPU

INSTANTIATE_TEST_SUITE_P(
    CompilerCommandLineTestsGpuInstance, CompilerCommandLineTests,
    testing::ValuesIn(prajna::tests::getFiles("tests/compiler/prajna_gpu_scripts")));

// 会遍历整个文件夹里的文件
INSTANTIATE_TEST_SUITE_P(
    CompilerSourceTestsGpuInstance, CompilerSourceTests,
    testing::ValuesIn(prajna::tests::getFiles("tests/compiler/prajna_gpu_sources")),
    PrintFileName());

#endif
