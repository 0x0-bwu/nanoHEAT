add_subdirectory(model)

add_library(nanoHEAT SHARED NanoHEAT.cpp)

target_link_libraries(nanoHEAT
    nanoDB
    nanoHeatModel
    ${MALLOC_LIB}
    ${PNG_LIBRARY} 
    Threads::Threads
    boost_serialization
    dl
)