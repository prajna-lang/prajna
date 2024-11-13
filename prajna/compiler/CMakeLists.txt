add_library(prajna_compiler compiler.cpp stdio_callback.cpp boost_exception.cpp)

target_link_libraries(prajna_compiler
    PRIVATE prajna_ir
    PRIVATE prajna_parser
    PRIVATE prajna_lexer
    PRIVATE prajna_grammar
    PRIVATE prajna_lowering
    PRIVATE prajna_transform
    PRIVATE prajna_codegen
    PRIVATE prajna_jit
    PUBLIC prajna_config_target
    PRIVATE llvm_include_dir
)
