add_library(nanoHeatModel
    utils/NSModelLayerStackupBuilder.cpp
    utils/NSModelPrismThermalBuilder.cpp
    NSModel.cpp
    NSModelLayerStackup.cpp
    NSModelPrismThermal.cpp
)

target_link_libraries(nanoHeatModel nanoDB)