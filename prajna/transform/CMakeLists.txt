add_library(prajna_transform OBJECT
    transform.cpp
)

target_link_libraries(prajna_transform
    PUBLIC prajna_ir
    PUBLIC prajna_parser
    PUBLIC prajna_lowering
)
