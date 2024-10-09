add_library(prajna_repl OBJECT repl.cpp)
target_link_libraries(prajna_repl
    PUBLIC prajna_compiler
    PUBLIC cpp-terminal
    PUBLIC cpp-terminal-platforms
)

target_include_directories(prajna_repl PUBLIC ${PROJECT_SOURCE_DIR}/tools)
