cmake_minimum_required(VERSION 3.10)

project(aco_solver CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include")
  set(OpenMP_CXX_LIB_NAMES "omp")
  set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")
endif()

find_package(OpenMP REQUIRED)

set(PYBIND11_FINDPYTHON ON)
add_subdirectory(third_party/pybind11)

file(GLOB SOURCES "src/*.cpp")

list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

pybind11_add_module(aco_solver ${SOURCES})

target_include_directories(aco_solver PRIVATE 
  include
  third_party
)

if(OpenMP_CXX_FOUND)
  target_link_libraries(aco_solver PRIVATE OpenMP::OpenMP_CXX)
endif()

set_target_properties(aco_solver PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")