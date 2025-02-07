add_library(nanoHeatModel
    utils/NSModelLayerStackupBuilder.cpp
    utils/NSModelLayerStackupQuery.cpp
    utils/NSModelLayoutPolygonMerger.cpp
    utils/NSModelPrismThermalBuilder.cpp
    utils/NSModelPrismThermalQuery.cpp
    NSModel.cpp
    NSModelLayerStackup.cpp
    NSModelPrismThermal.cpp
)

target_link_libraries(nanoHeatModel nanoDB)