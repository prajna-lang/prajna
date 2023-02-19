add_library(prajna_repl repl.cpp)
target_link_libraries(prajna_repl
    PRIVATE prajna_compiler
    PRIVATE cpp-terminal
)

target_include_directories(prajna_repl PUBLIC ${PROJECT_SOURCE_DIR}/tools)
