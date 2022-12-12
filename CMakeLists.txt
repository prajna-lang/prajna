cmake_minimum_required(VERSION 3.14)

project(Prajna CXX C)

set(CMAKE_CXX_STANDARD 17) # c++20目前系统库还不支持, 没有必要
set(CXX_STANDARD_REQUIRED ON)  # 若不支持c++17选项则报错，而不是使用c++14
set(CMAKE_CXX_EXTENSIONS OFF)  # 使用 -std=c++17而不是 -std=gnu++17


# 在mac平台下， 因为llvm设置为hidden，所以避免编译warning这里全局设置为hidden
# 之所以不用target_link_options设置，是因为其不能设置第三方项目
if (NOT BUILD_SHARED_LIBS)
   set(CMAKE_CXX_VISIBILITY_PRESET hidden)
   set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
else()
# 没有找到动态库的warning消除方法
endif()

set(LLVM_INCLUDE_TESTS OFF)

add_subdirectory(third_party/llvm-project/llvm EXCLUDE_FROM_ALL)
# 需要手动添加llvm的头文件路径
add_library(llvm_include_dir INTERFACE)
target_include_directories(llvm_include_dir
    INTERFACE ${PROJECT_SOURCE_DIR}/third_party/llvm-project/llvm/include
    INTERFACE ${PROJECT_BINARY_DIR}/third_party/llvm-project/llvm/include
)

add_library(clang_include_dir INTERFACE)
target_include_directories(clang_include_dir
    INTERFACE ${PROJECT_SOURCE_DIR}/third_party/llvm-project/llvm/tools/clang/include
    INTERFACE ${PROJECT_BINARY_DIR}/third_party/llvm-project/llvm/tools/clang/include
)

target_link_libraries(clang_include_dir
   INTERFACE llvm_include_dir
)


add_subdirectory(third_party/boost EXCLUDE_FROM_ALL)
add_subdirectory(third_party/cxxopts EXCLUDE_FROM_ALL)
add_subdirectory(third_party/fmt EXCLUDE_FROM_ALL)
add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(PRAJNA_BUILD_TESTS "Build Prajna unit tests" ON)
option(PRAJNA_BUILD_TOOLS "Build Prajna tools" ON)
option(PRAJNA_ENABLE_SANITIZE "Enable Prajna address sanitize" OFF)
option(PRAJNA_WITH_GPU "With CUDA" OFF)
option(PRAAJNA_ENABLE_LLVM_DUMP "Enable Prajna LLVM dump" OFF)
option(PRAAJNA_ENABLE_LLVM_VERIFY "Enable Prajna LLVM verify" OFF)

add_subdirectory(prajna)
# add_subdirectory(paramita)


if(PRAJNA_BUILD_TESTS)
   add_subdirectory(tests)
endif()

if(PRAJNA_BUILD_TOOLS)
   add_subdirectory(tools)
endif()

# install(TARGETS prajna
#         RUNTIME DESTINATION bin)

