add_library(prajna_lowering
    lower.cpp
    symbol_table.cpp
)

target_link_libraries(prajna_lowering
    PUBLIC prajna_config_target
    PUBLIC prajna_ast
    PUBLIC prajna_ir
)
