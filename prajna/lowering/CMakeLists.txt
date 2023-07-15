add_library(prajna_lowering
    lower.cpp
    statement_lowering_visitor.cpp
    symbol_table.cpp
)

target_link_libraries(prajna_lowering
    PUBLIC prajna_config_target
    PUBLIC prajna_ast
    PUBLIC prajna_ir
    PRIVATE llvm_include_dir
)
