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

namespace prajna::jit {

class GpuCompiler {
   public:
    GpuCompiler(ir::Target target_type) : target_type(target_type) {}
    std::string CompileToPTXCode(llvm::Module* llvm_module, std::string name);
    std::string CompileToHSACO(llvm::Module* llvm_module, std::string name);

   private:
    ir::Target target_type;
};

inline std::string GetTimeStr() {
    // 生成时间字符串：20240622_194540
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream time_ss;
    time_ss << std::put_time(std::localtime(&now_time_t), "_%Y%m%d_%H%M%S");
    return time_ss.str();
}

inline std::tuple<std::string, std::string> GetTargetGPUArchitecture(ir::Target target) {
    auto& config = prajna::GlobalConfig::Instance();
    std::string triple;
    std::string sm_version;

    // 动态生成配置键
    std::string target_key = "target.triple." + ir::TargetToString(target);
    triple = config.get<std::string>(target_key, "");

    std::array<char, 128> buffer;
    std::string result;
#ifdef __linux__
    if (target == prajna::ir::Target::nvptx) {
        FILE* pipe = popen("nvidia-smi --query-gpu=compute_cap --format=csv,noheader", "r");
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                result += buffer.data();
            }
            pclose(pipe);
            // 移除换行符和多余空格
            result.erase(std::remove_if(result.begin(), result.end(),
                                        [](char c) { return c == '\n' || c == ' '; }),
                         result.end());
            if (!result.empty()) {
                result.erase(std::remove(result.begin(), result.end(), '.'), result.end());
                sm_version = fmt::format("sm_{}", result);
            }
        }
    } else if (target == prajna::ir::Target::amdgpu) {
        std::array<char, 128> buffer;
        std::string result;
        FILE* pipe = popen("rocminfo | grep 'Name:.*gfx' | head -1 | awk '{print $2}'", "r");
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                result += buffer.data();
            }
            pclose(pipe);
            result.erase(std::remove_if(result.begin(), result.end(),
                                        [](char c) { return c == '\n' || c == ' '; }),
                         result.end());
            if (!result.empty()) {
                sm_version = result;  // 例如 "gfx906"
            }
        }
    }

#endif

// todo
#ifdef _WIN32
    // Windows 环境下通过配置文件或其他工具获取架构
    // 示例：假设通过环境变量或专用工具获取
    if (target == ir::Target::nvptx) {
        // 可添加 Windows 下的 nvidia-smi 替代方案
    } else if (target == ir::Target::amdgpu) {
        // 可添加 Windows 下的 rocminfo 替代方案
    }
#endif

    return {triple, sm_version};
}

std::string GpuCompiler::CompileToPTXCode(llvm::Module* llvm_module,std::string name) {
    llvm::LLVMContext& llvm_context = llvm_module->getContext();

    // 设置目标 triple 和数据布局
    auto [target_triple, sm_version] = GetTargetGPUArchitecture(this->target_type);

    std::string error_msg;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, error_msg);
    PRAJNA_ASSERT(target, "Target not found: " + error_msg);

    llvm::TargetOptions opts;
    std::unique_ptr<llvm::TargetMachine> target_machine(
        target->createTargetMachine(target_triple, sm_version, "", opts, llvm::Reloc::PIC_));

    llvm_module->setTargetTriple(target_triple);
    llvm_module->setDataLayout(target_machine->createDataLayout());

    // 生成 Assembly code
    llvm::SmallString<0> buffer;
    llvm::raw_svector_ostream ostream(buffer);
    llvm::legacy::PassManager pass_manager;
    llvm::CodeGenFileType file_type;
    if (target_type == ir::Target::nvptx) {
        file_type = llvm::CodeGenFileType::AssemblyFile;
    } else if (target_type == ir::Target::amdgpu) {
        file_type = llvm::CodeGenFileType::ObjectFile;
    } else {
        PRAJNA_UNREACHABLE;
    }

    PRAJNA_ASSERT(!target_machine->addPassesToEmitFile(pass_manager, ostream, nullptr, file_type),
                  "Cannot emit ");

    pass_manager.run(*llvm_module);

    std::string ptx = buffer.str().str();
    std::string file_ext = ".ptx";
    std::string file_base =
        (std::filesystem::temp_directory_path() / std::filesystem::path(name).stem())
            .string();
    file_base += GetTimeStr();
    std::error_code err_code;
    llvm::raw_fd_ostream ptx_fs(file_base + file_ext, err_code);
    PRAJNA_ASSERT(!err_code && "Failed to open file for  output");
    ptx_fs << ptx;
    ptx_fs.close();
    return file_base;
}

/**
 * 参考文档：
 * https://oneapi-src.github.io/unified-runtime/core/HIP.html
 * Unlike the NVPTX platform, AMDGPU does not use a device IR that can be JIT compiled at runtime.
 * Therefore, all device binaries must be precompiled for a particular arch.
 * https://llvm.org/docs/AMDGPUUsage.html#elf-code-object
 * The AMDGPU backend generates a standard ELF [ELF] relocatable code object that can be linked by
 * lld to produce a standard ELF shared code object which can be loaded and executed on an AMDGPU
 * target.
 */
std::string GpuCompiler::CompileToHSACO(llvm::Module* llvm_module, std::string name) {
    llvm::LLVMContext& llvm_context = llvm_module->getContext();

    auto [target_triple, sm_version] = GetTargetGPUArchitecture(prajna::ir::Target::amdgpu);

    std::string error_msg;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, error_msg);
    PRAJNA_ASSERT(target, "Target not found: " + error_msg);

    llvm::TargetOptions opts;
    std::unique_ptr<llvm::TargetMachine> target_machine(
        target->createTargetMachine(target_triple, sm_version, "", opts, llvm::Reloc::PIC_));

    // Set target triple and data layout
    llvm_module->setTargetTriple(target_triple);
    llvm_module->setDataLayout(target_machine->createDataLayout());

    // 输出 IR 供调试用
    // std::string ir_str;
    // llvm::raw_string_ostream ir_ostream(ir_str);
    // llvm_module->print(ir_ostream, nullptr);
    // std::cerr << "LLVM IR:\n" << ir_str << std::endl;

    // 路径准备
    std::string file_base =
        (std::filesystem::temp_directory_path() / std::filesystem::path(name).stem()).string();
    file_base += GetTimeStr();

    std::string obj_path = file_base + ".o";
    std::string hsaco_path = file_base + ".hsaco";

    // 1. 输出 .o 文件
    std::error_code ec;
    llvm::raw_fd_ostream obj_out(obj_path, ec, llvm::sys::fs::OF_None);
    PRAJNA_ASSERT(!ec, "Failed to open .o file for writing");

    llvm::legacy::PassManager pass_manager;
    bool fail = target_machine->addPassesToEmitFile(pass_manager, obj_out, nullptr,
                                                    llvm::CodeGenFileType::ObjectFile);
    PRAJNA_ASSERT(!fail, "Failed to add emit passes for object file");

    pass_manager.run(*llvm_module);
    obj_out.flush();
    obj_out.close();

    // 2. 链接 .o -> .hsaco
    std::string cmd = fmt::format("ld.lld -shared --no-undefined {} -o {}", obj_path, hsaco_path);
    int result = std::system(cmd.c_str());
    PRAJNA_ASSERT(result == 0, "ld.lld linking failed: command = " + cmd);
    PRAJNA_ASSERT(std::filesystem::exists(hsaco_path), "hsaco not generated");

    return hsaco_path;
}

}  // namespace prajna::jit