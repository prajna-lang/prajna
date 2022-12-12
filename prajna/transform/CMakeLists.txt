add_library(prajna_transform
    transform.cpp
)

target_link_libraries(prajna_transform
    PUBLIC prajna_ir
)
