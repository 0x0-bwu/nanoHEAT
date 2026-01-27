add_library(nano_heat_solver
    utils/NSPrismStackupThermalNetworkBuilder.cpp
    utils/NSPrismThermalNetworkBuilder.cpp
    NSSolverPrismThermalNetwork.cpp
)

target_link_libraries(nano_heat_solver nano_db)