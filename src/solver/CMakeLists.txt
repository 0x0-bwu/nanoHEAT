add_library(nanoHeatSolver
    utils/NSPrismThermalNetworkBuilder.cpp
    NSSolverPrismThermalNetwork.cpp
)

target_link_libraries(nanoHeatSolver nanoDB)