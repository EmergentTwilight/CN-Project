# Build algorithm library
add_library(algorithm-lib STATIC shortest-path-algorithms.cc)

# Include directory for this library
target_include_directories(algorithm-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})