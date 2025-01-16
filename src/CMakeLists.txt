add_subdirectory(model)

add_library(nanoHEAT SHARED NanoHEAT.cpp)

target_link_libraries(nanoHEAT
    nanoHeatModel
)