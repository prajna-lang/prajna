#pragma once

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include <string>

#include "prajna/assert.hpp"

// 使用 LLVM 生成 PTX 字符串
class GpuCompiler {
   public:
    std::string CompileToPTX(llvm::Module* llvm_module);
};

inline std::tuple<std::string, std::string> GetTargetGPUArchitecture() {
    auto& config = prajna::GlobalConfig::Instance();
    std::string triple = config.get<std::string>("target.triple", "");
    std::string cpu = config.get<std::string>("target.cpu", "");
    return {triple, cpu};
}

std::string GpuCompiler::CompileToPTX(llvm::Module* llvm_module) {
    llvm::LLVMContext& llvm_context = llvm_module->getContext();

    //=== 调试@intrinsic("__nv_sinf")代码 ,如果没有用可以删掉 ===
    // 加载 libdevice bitcode 文件
    llvm::SMDiagnostic err;
    std::string libdevice_path = "/usr/local/cuda-12.6/nvvm/libdevice/libdevice.10.bc";
    auto libdevice_module = llvm::parseIRFile(libdevice_path, err, llvm_context);
    PRAJNA_ASSERT(libdevice_module, "Failed to load libdevice bitcode from " + libdevice_path);

    // 链接 libdevice 到主模块
    // llvm::Linker::Flags::OverrideFromSrc:让 libdevice.10.bc
    // 中的函数覆盖掉主模块中的函数定义，避免重复定义
    bool link_failed = llvm::Linker::linkModules(
        *llvm_module, std::move(libdevice_module),
        llvm::Linker::Flags::OverrideFromSrc | llvm::Linker::Flags::LinkOnlyNeeded);
    PRAJNA_ASSERT(!link_failed, "Failed to link libdevice module");

    for (auto& f : llvm_module->functions()) {
        if (f.getLinkage() == llvm::GlobalValue::AvailableExternallyLinkage) {
            f.setLinkage(llvm::GlobalValue::ExternalLinkage);
        }
    }

    //=== 调试代码 end ===

    // 设置目标 triple 和数据布局
    auto [target_triple, cpu] = GetTargetGPUArchitecture();

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, error);
    PRAJNA_ASSERT(target, "Target not found: " + error);

    llvm::TargetOptions opts;
    std::unique_ptr<llvm::TargetMachine> target_machine(
        target->createTargetMachine(target_triple, cpu, "", opts, llvm::Reloc::PIC_));

    llvm_module->setTargetTriple(target_triple);
    llvm_module->setDataLayout(target_machine->createDataLayout());

    // 生成 PTX 代码
    llvm::SmallString<0> buffer;
    llvm::raw_svector_ostream ostream(buffer);
    llvm::legacy::PassManager pm;
    PRAJNA_ASSERT(
        !target_machine->addPassesToEmitFile(pm, ostream, nullptr, llvm::CGFT_AssemblyFile),
        "Cannot emit PTX");

    pm.run(*llvm_module);
    return buffer.str().str();
}