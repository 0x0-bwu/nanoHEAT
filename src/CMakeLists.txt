add_library(nanoHEAT SHARED NanoHEAT.cpp)

target_link_libraries(nanoHEAT
    nanoDB
    ${MALLOC_LIB}
    ${PNG_LIBRARY} 
    Threads::Threads
    boost_serialization
    dl
)