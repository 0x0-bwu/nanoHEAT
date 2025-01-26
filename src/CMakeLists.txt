add_subdirectory(model)
add_subdirectory(solver)
add_subdirectory(simulation)

add_library(nanoHEAT SHARED NanoHEAT.cpp)

target_link_libraries(nanoHEAT
    nanoHeatModel
    nanoHeatSolver
    nanoHeatSimulation
)