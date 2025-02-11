add_library(nanoHeatModel
    utils/NSModelLayerStackupBuilder.cpp
    utils/NSModelLayerStackupQuery.cpp
    utils/NSModelLayoutPolygonMerger.cpp
    utils/NSModelPrismStackupThermalBuilder.cpp
    utils/NSModelPrismStackupThermalQuery.cpp
    utils/NSModelPrismThermalBuilder.cpp
    utils/NSModelPrismThermalQuery.cpp
    NSModel.cpp
    NSModelLayerStackup.cpp
    NSModelPrismStackupThermal.cpp
    NSModelPrismThermal.cpp
)

target_link_libraries(nanoHeatModel nanoDB)