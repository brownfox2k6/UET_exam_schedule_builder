cmake_minimum_required(VERSION 3.19)

project(aco_solver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
    set(BREW_PREFIX "/opt/homebrew") # Mac Apple Silicon
  else()
    set(BREW_PREFIX "/usr/local")    # Mac Intel
  endif()

  set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I${BREW_PREFIX}/opt/libomp/include")
  set(OpenMP_CXX_LIB_NAMES "omp")
  set(OpenMP_omp_LIBRARY "${BREW_PREFIX}/opt/libomp/lib/libomp.dylib")
endif()

find_package(OpenMP REQUIRED)

include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v3.0.4
)
set(PYBIND11_FINDPYTHON ON)
FetchContent_MakeAvailable(pybind11)

file(GLOB SOURCES "src/*.cpp")

list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

pybind11_add_module(aco_solver ${SOURCES})

target_include_directories(aco_solver PRIVATE include)

if(OpenMP_CXX_FOUND)
  target_link_libraries(aco_solver PRIVATE OpenMP::OpenMP_CXX)
endif()

set_target_properties(aco_solver PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")