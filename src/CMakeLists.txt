add_subdirectory(model)
add_subdirectory(solver)
add_subdirectory(simulation)

add_library(nano_heat SHARED NanoHEAT.cpp)

target_link_libraries(nano_heat
    nano_heat_model
    nano_heat_simulation
)