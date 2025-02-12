add_library(nanoHeatSolver
    utils/NSPrismStackupThermalNetworkBuilder.cpp
    utils/NSPrismThermalNetworkBuilder.cpp
    NSSolverPrismThermalNetwork.cpp
)

target_link_libraries(nanoHeatSolver nanoDB)