add_library(prajna_repl OBJECT repl.cpp)
target_link_libraries(prajna_repl
    PUBLIC prajna_compiler
    PRIVATE cpp-terminal
)

target_include_directories(prajna_repl PUBLIC ${PROJECT_SOURCE_DIR}/tools)
