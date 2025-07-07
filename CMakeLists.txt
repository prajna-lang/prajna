cmake_minimum_required(VERSION 3.14)

project(prajna CXX C)

set(CMAKE_WARN_DEPRECATED OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # 用于智能提示
set(CMAKE_CXX_STANDARD 23) # c++20目前系统库还不支持, 没有必要
set(CXX_STANDARD_REQUIRED ON)  # 若不支持c++17选项则报错，而不是使用c++14

# 在mac平台下， 因为llvm设置为hidden，所以避免编译warning这里全局设置为hidden
# 之所以不用target_link_options设置，是因为其不能设置第三方项目
if (NOT BUILD_SHARED_LIBS)
   set(CMAKE_CXX_VISIBILITY_PRESET hidden)
   set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
else()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# 没有找到动态库的warning消除方法
endif()

set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

if (UNIX AND NOT APPLE)
    # llvm compiler-rt不支持"Linux", 所以需要改为"linxu-gnu"
    set(CMAKE_SYSTEM_NAME "linux-gnu")

    # llvm compiler-rt需要, 交叉编译需要重新设置
    set(CMAKE_C_COMPILER_TARGET "${CMAKE_SYSTEM_PROCESSOR}-unknown-${CMAKE_SYSTEM_NAME}")
    # 不可设置C++的, 会导致头文件找不到
    # set(CMAKE_CXX_COMPILER_TARGET "${CMAKE_SYSTEM_PROCESSOR}-unknown-${CMAKE_SYSTEM_NAME}")
    set(COMPILER_RT_BUILTINS_HIDE_SYMBOLS Off)
    set(COMPILER_RT_BUILD_BUILTINS ON)
    set(COMPILER_RT_DEFAULT_TARGET_ONLY ON)
else()
    # do none
endif()

if (MSVC)
    add_compile_options(/wd4996)
else()
    add_compile_options(-Wno-deprecated-this-capture)
endif()


message("prajna build target triple: ${CMAKE_C_COMPILER_TARGET}")
add_subdirectory(third_party/llvm-project/llvm EXCLUDE_FROM_ALL)
# add_subdirectory(third_party/llvm-project/compiler-rt)
# 需要手动添加llvm的头文件路径
add_library(llvm_include_dir INTERFACE)
target_include_directories(llvm_include_dir
    INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/llvm-project/llvm/include>
    $<INSTALL_INTERFACE:include>
    INTERFACE
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/third_party/llvm-project/llvm/include>
    $<INSTALL_INTERFACE:include>
)
add_dependencies(llvm_include_dir intrinsics_gen)

add_subdirectory(third_party/boost EXCLUDE_FROM_ALL)
add_subdirectory(third_party/cxxopts EXCLUDE_FROM_ALL)
add_subdirectory(third_party/fmt EXCLUDE_FROM_ALL)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)

set(CPPTERMINAL_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/cpp-terminal EXCLUDE_FROM_ALL)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(BUILD_SHARED_LIBS "Build shared libs" OFF) # 需要加入, 否则其并不是一个option
option(PRAJNA_BUILD_TOOLS "Build Prajna tools" ON)
option(PRAJNA_BUILD_EXAMPLES "Build Prajna examples" ON)
option(PRAJNA_BUILD_TESTS "Build Prajna tests" ON)
option(PRAJNA_WITH_JUPYTER "Build Jupyter Xeus Prajna" OFF)
option(PRAJNA_ENABLE_SANITIZE "Enable Prajna address sanitize" OFF)
option(PRAJNA_DISABLE_ASSERT "Disable Prajna ASSERTS" OFF)
option(PRAJNA_WITH_CUDA "Enable CUDA support" OFF)

add_subdirectory(prajna)
if (PRAJNA_WITH_JUPYTER)
    add_subdirectory(xeus_prajna)
endif()

if (PRAJNA_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if (PRAJNA_BUILD_TOOLS)
    # nlohmann_json会在xeus_prajna里被加入, 所以需要判断
    if (NOT TARGET nlohmann_json)
        add_subdirectory(xeus_prajna/third_party/json EXCLUDE_FROM_ALL)
    endif()

    add_subdirectory(tools)
endif()

add_subdirectory(examples_in_cpp)

include(CMakePackageConfigHelpers)

install(TARGETS prajna EXPORT ${PROJECT_NAME}-targets)
# xeus_prajna依赖
install(TARGETS nlohmann_json EXPORT ${PROJECT_NAME}-targets)
install(DIRECTORY builtin_packages DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY dockerfiles DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY docs DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY examples DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES prajna/compiler/compiler.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
