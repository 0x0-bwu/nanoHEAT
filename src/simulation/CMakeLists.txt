add_library(nano_heat_simulation
    NSSimulationPrismThermal.cpp
)

target_link_libraries(nano_heat_simulation nano_db nano_heat_solver)