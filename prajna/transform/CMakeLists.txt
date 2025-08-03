add_library(prajna_transform INTERFACE)

target_link_libraries(prajna_transform
    INTERFACE prajna_ir
    INTERFACE prajna_parser
    INTERFACE prajna_lowering
)
