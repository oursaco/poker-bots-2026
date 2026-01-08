cmake_minimum_required(VERSION 3.16)

# Prefer GCC/G++ unless a compiler is explicitly set by the user/toolchain.
if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_CXX_COMPILER g++)
endif()

project(pokerbots_2026 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-O3 -std=c++17 -pthread -march=native -ffast-math)

if(MSVC)
    add_compile_options(/W0)
else()
    add_compile_options(-w)
endif()
add_link_options(-Wl,--stack,2147483648)

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

add_executable(visualize_infosets tools/visualize_infosets.cpp)
target_link_libraries(visualize_infosets PRIVATE pokerbots_headers omp)

add_executable(visualize_tree tools/visualize_tree.cpp)
target_link_libraries(visualize_tree PRIVATE pokerbots_headers)

add_executable(visualize_utility tools/visualize_utility.cpp)
target_link_libraries(visualize_utility PRIVATE pokerbots_headers omp)

add_executable(visualize_strategy tools/visualize_strategy.cpp)
target_link_libraries(visualize_strategy PRIVATE pokerbots_headers omp)

add_executable(khun_trainer trainers/khun_trainer.cpp)
target_link_libraries(khun_trainer PRIVATE pokerbots_headers)

add_executable(poker_trainer trainers/poker_trainer.cpp)
target_link_libraries(poker_trainer PRIVATE pokerbots_headers omp)

add_executable(three_card_trainer trainers/three_card_trainer.cpp)
target_link_libraries(three_card_trainer PRIVATE pokerbots_headers omp)



