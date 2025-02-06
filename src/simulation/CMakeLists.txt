add_library(nanoHeatSimulation
    NSSimulationPrismThermal.cpp
)

target_link_libraries(nanoHeatSimulation nanoDB nanoHeatSolver)