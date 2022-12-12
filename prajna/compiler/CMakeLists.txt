add_library(prajna_compiler compiler.cpp)

target_link_libraries(prajna_compiler
    PUBLIC prajna_parser
    PUBLIC prajna_lowering
    PUBLIC prajna_transform
    PUBLIC prajna_codegen
    PUBLIC prajna_jit
    PUBLIC prajna_config_target
    PUBLIC llvm_include_dir
)
