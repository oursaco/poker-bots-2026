cmake_minimum_required(VERSION 3.20)

project(pokerbots_2026 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pokerbots_headers INTERFACE)
target_include_directories(pokerbots_headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(omp STATIC
    external/omp/CardRange.cpp
    external/omp/CombinedRange.cpp
    external/omp/EquityCalculator.cpp
    external/omp/HandEvaluator.cpp
)
target_include_directories(omp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/omp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libdivide
)

add_executable(visualize_tree tools/visualize_tree.cpp)
target_link_libraries(visualize_tree PRIVATE pokerbots_headers)

add_executable(khun_trainer tools/khun_trainer.cpp)
target_link_libraries(khun_trainer PRIVATE pokerbots_headers)