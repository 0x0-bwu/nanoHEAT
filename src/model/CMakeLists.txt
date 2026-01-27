add_library(nano_heat_model
    utils/NSModelLayerStackupBuilder.cpp
    utils/NSModelLayerStackupQuery.cpp
    utils/NSModelLayoutPolygonMerger.cpp
    utils/NSModelPrismMeshGenerator.cpp
    utils/NSModelPrismMeshGmshUtils.cpp
    utils/NSModelPrismStackupThermalBuilder.cpp
    utils/NSModelPrismStackupThermalQuery.cpp
    utils/NSModelPrismThermalBuilder.cpp
    utils/NSModelPrismThermalQuery.cpp
    NSModel.cpp
    NSModelLayerStackup.cpp
    NSModelPrismStackupThermal.cpp
    NSModelPrismThermal.cpp
)

target_link_libraries(nano_heat_model nano_db)