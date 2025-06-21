// cuda_runtime_loader.hpp
#pragma once

#include <cuda.h>
#include <iostream>
#include <string>
#include "prajna/assert.hpp"

//  CUDA Driver API 加载 PTX 并准备运行时上下文
class CudaRuntimeLoader {
   public:
    bool Initialize();
    CUmodule LoadPTX(const std::string& ptx_code);
    CUfunction GetKernel(CUmodule module, const std::string& name);

   private:
    bool InitDriver();
    void InitContext();
    void Report(CUresult result);

    CUcontext context;
};

bool CudaRuntimeLoader::Initialize() {
    PRAJNA_ASSERT(InitDriver());
    InitContext();
    return true;
}

bool CudaRuntimeLoader::InitDriver() {
    PRAJNA_ASSERT(cuInit(0) == CUDA_SUCCESS, "cuInit failed");
    return true;
}

void CudaRuntimeLoader::InitContext() {
    CUdevice device;
    PRAJNA_ASSERT(cuDeviceGet(&device, 0) == CUDA_SUCCESS, "cuDeviceGet failed");
    PRAJNA_ASSERT(cuCtxCreate(&context, 0, device) == CUDA_SUCCESS, "cuCtxCreate failed");
}

CUmodule CudaRuntimeLoader::LoadPTX(const std::string& ptx_code) {
    CUmodule module;
    CUresult res = cuModuleLoadDataEx(&module, ptx_code.c_str(), 0, nullptr, nullptr);
    PRAJNA_ASSERT(res == CUDA_SUCCESS, "cuModuleLoadDataEx failed: " + std::to_string(res));
    return module;
}

CUfunction CudaRuntimeLoader::GetKernel(CUmodule module, const std::string& name) {
    CUfunction function;
    CUresult res = cuModuleGetFunction(&function, module, name.c_str());
    PRAJNA_ASSERT(res == CUDA_SUCCESS, "cuModuleGetFunction failed: " + std::to_string(res));
    return function;
}