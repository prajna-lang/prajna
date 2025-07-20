#pragma once

#include <boost/dll/shared_library.hpp>
#include <functional>
#include <iostream>

class CudaRuntimeLoader {
   public:
    // 获取单例实例
    static CudaRuntimeLoader& GetInstance() {
        static CudaRuntimeLoader instance;
        return instance;
    }

    // 获取shared_library对象
    boost::dll::shared_library& GetLibrary() { return GetCudaLibrary(); }

    // 获取CUDA函数指针
    using CuInitFunc = int(int);
    using CuLibraryLoadFunc = int(int64_t*, const char*, int*, int*, int, int*, int*, int);
    using CuLibraryGetKernelFunc = int(int64_t*, int64_t, const char*);

    CuInitFunc* GetCuInit() { return GetCudaLibrary().get<CuInitFunc>("cuInit"); }

    CuLibraryLoadFunc* GetCuLibraryLoadFromFile() {
        return GetCudaLibrary().get<CuLibraryLoadFunc>("cuLibraryLoadFromFile");
    }

    CuLibraryGetKernelFunc* GetCuLibraryGetKernel() {
        return GetCudaLibrary().get<CuLibraryGetKernelFunc>("cuLibraryGetKernel");
    }

    // cu_library_load_from_file 调用
    int64_t LoadCudaLibraryFromFile(const std::string& ptx_file) {
        int64_t cu_library = 0;
        auto cu_library_load_from_file = GetCuLibraryLoadFromFile();
        auto cu_re2 = cu_library_load_from_file(&cu_library, ptx_file.c_str(), nullptr, nullptr, 0,
                                                nullptr, nullptr, 0);
        PRAJNA_ASSERT(cu_re2 == 0, "cuLibraryLoadFromFile failed: " + std::to_string(cu_re2));
        return cu_library;
    }

   private:
    CudaRuntimeLoader() {
        // 初始化CUDA上下文
        int cu_result = GetCuInit()(0);
        PRAJNA_ASSERT(cu_result == 0, "cuInit failed: " + std::to_string(cu_result));
    }

    static boost::dll::shared_library& GetCudaLibrary() {
        static boost::dll::shared_library cuda_so = []() {
#ifdef _WIN32
            std::cout << "Loading nvcuda.dll" << std::endl;
            return boost::dll::shared_library("C:\\Windows\\System32\\nvcuda.dll");
#else
            std::cout << "Loading libcuda.so" << std::endl;
            return boost::dll::shared_library("/usr/lib/x86_64-linux-gnu/libcuda.so");
#endif
        }();
        return cuda_so;
    }
};
