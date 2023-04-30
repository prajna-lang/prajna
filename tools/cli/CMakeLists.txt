add_executable(prajna prajna_main.cpp)
target_link_libraries(prajna
    prajna_repl
    prajna_compiler
    Boost::process
    Boost::asio
    cxxopts
    nlohmann_json
    fmt
)

if (BUILD_SHARED_LIBS)
    target_link_libraries(prajna ${CMAKE_DL_LIBS})
endif()
