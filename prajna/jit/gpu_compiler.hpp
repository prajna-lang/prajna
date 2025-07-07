#pragma once

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include <ranges>
#include <string>

#include "prajna/assert.hpp"
#include "prajna/global_config.hpp"

// 使用 LLVM 生成 PTX 字符串
class GpuCompiler {
   public:
    std::string CompileToPTX(llvm::Module* llvm_module);
};

inline std::tuple<std::string, std::string> GetTargetGPUArchitecture() {
    auto& config = prajna::GlobalConfig::Instance();
    std::string triple = config.get<std::string>("target.triple", "");
    std::string sm_version;

    std::array<char, 128> buffer;
    std::string result;
#ifdef __linux__
    FILE* pipe = popen("nvidia-smi --query-gpu=compute_cap --format=csv,noheader", "r");
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        pclose(pipe);
        // 移除换行符和多余空格
        auto new_end = std::ranges::remove_if(result, [](char c) { return c == '\n' || c == ' '; });
        result.erase(new_end.begin(), result.end());
        if (!result.empty()) {
            auto new_end2 = std::ranges::remove(result, '.');
            result.erase(new_end2.begin(), result.end());
            sm_version = fmt::format("sm_{}", result);
        }
    }
#endif
    return {triple, sm_version};
}

std::string GpuCompiler::CompileToPTX(llvm::Module* llvm_module) {
    llvm::LLVMContext& llvm_context = llvm_module->getContext();

    // 设置目标 triple 和数据布局
    auto [target_triple, cpu] = GetTargetGPUArchitecture();

    std::string error_msg;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, error_msg);
    PRAJNA_ASSERT(target, "Target not found: " + error_msg);

    llvm::TargetOptions opts;
    std::unique_ptr<llvm::TargetMachine> target_machine(
        target->createTargetMachine(target_triple, cpu, "", opts, llvm::Reloc::PIC_));

    llvm_module->setTargetTriple(target_triple);
    llvm_module->setDataLayout(target_machine->createDataLayout());

    // 生成 PTX 代码
    llvm::SmallString<0> buffer;
    llvm::raw_svector_ostream ostream(buffer);
    llvm::legacy::PassManager pass_manager;
    PRAJNA_ASSERT(!target_machine->addPassesToEmitFile(pass_manager, ostream, nullptr,
                                                       llvm::CodeGenFileType::AssemblyFile),
                  "Cannot emit PTX");

    pass_manager.run(*llvm_module);
    return buffer.str().str();
}
