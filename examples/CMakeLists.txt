set(BINARY_NAME JetsonNano.exe)
add_executable(${BINARY_NAME} kicad/JetsonNano.cpp)
target_include_directories(${BINARY_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(${BINARY_NAME} PRIVATE nanoDB nanoHEAT)

set(BINARY_NAME TinyTapeOut.exe)
add_executable(${BINARY_NAME} kicad/TinyTapeOut.cpp)
target_include_directories(${BINARY_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(${BINARY_NAME} PRIVATE nanoDB nanoHEAT)
