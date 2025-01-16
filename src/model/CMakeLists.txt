add_library(nanoHeatModel
    utils/NSModelLayerStackupBuilder.cpp
    NSModelLayerStackup.cpp
    NSModel.cpp
)

target_link_libraries(nanoHeatModel nanoDB)