
add_library(prajna_config_target INTERFACE)
target_include_directories(prajna_config_target INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(prajna_config_target
    INTERFACE Boost::variant
    INTERFACE Boost::optional
    INTERFACE Boost::fusion
    INTERFACE Boost::algorithm
    INTERFACE Boost::dll
    INTERFACE Boost::process
    INTERFACE Boost::asio
    INTERFACE Boost::scope
    INTERFACE Boost::property_tree
    INTERFACE fmt::fmt
    INTERFACE nlohmann_json
)

target_compile_options(prajna_config_target INTERFACE -std=c++23)

target_compile_definitions(prajna_config_target
    # 参阅third_party/boost/libs/mpl/include/boost/mpl/list/list50.hpp
    # 并非任意数字都行, 需要是10,20,30,40,50
    # boost::variant的模板参数受此影响
    INTERFACE BOOST_MPL_LIMIT_LIST_SIZE=50
    INTERFACE BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
)

if (MSVC)
  target_compile_options(prajna_config_target INTERFACE "/bigobj")
else ()
    target_compile_options(prajna_config_target
        INTERFACE -Wno-unused-parameter
        INTERFACE -Wno-deprecated-declarations
    )
endif ()

# if (NOT PRAJNA_WITH_JUPYTER AND NOT MSVC)
#     target_compile_options(prajna_config_target
#         INTERFACE -Werror
#     )
# endif()




if (PRAJNA_DISABLE_ASSERT)
    target_compile_definitions(prajna_config_target INTERFACE PRAJNA_DISABLE_ASSERT=1)
endif()

# 需要禁止一些函数才能很好工作, mac下不支持detect-leaks
# 对于内存泄露来说, valgrind可能更加友好一点
if (PRAJNA_ENABLE_SANITIZE)
    target_compile_options(prajna_config_target
        INTERFACE -fsanitize=address
        INTERFACE -fno-omit-frame-pointer)

    target_link_options(prajna_config_target
        INTERFACE -fsanitize=address
        INTERFACE -fno-omit-frame-pointer
        INTERFACE -static-libsan)
endif()

add_subdirectory(ast)
add_subdirectory(ir)
add_subdirectory(lowering)
add_subdirectory(parser)
add_subdirectory(codegen)
add_subdirectory(transform)
add_subdirectory(jit)
add_subdirectory(compiler)

add_library(prajna_core INTERFACE)

target_link_libraries(prajna_core
    INTERFACE prajna_config_target
    INTERFACE prajna_compiler
)
