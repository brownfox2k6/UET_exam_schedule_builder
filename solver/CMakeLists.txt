cmake_minimum_required(VERSION 3.19)
project(aco_solver CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(IWYU_PATH NAMES include-what-you-use iwyu)
if(IWYU_PATH)
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_PATH}")
endif()

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
    set(BREW_PREFIX "/opt/homebrew") # Mac Apple Silicon
  else()
    set(BREW_PREFIX "/usr/local")    # Mac Intel
  endif()

  list(APPEND CMAKE_PREFIX_PATH "${BREW_PREFIX}/opt/libomp")
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
  list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
endif()

list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})

pybind11_add_module(aco_solver ${SOURCES})

target_link_libraries(aco_solver PRIVATE Python3::Python)

target_include_directories(aco_solver PRIVATE include)

if(OpenMP_CXX_FOUND)
  target_link_libraries(aco_solver PRIVATE OpenMP::OpenMP_CXX)
endif()

set_target_properties(aco_solver PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Tìm lệnh pybind11-stubgen
find_program(STUBGEN_EXE pybind11-stubgen)

if(STUBGEN_EXE)
  add_custom_command(TARGET aco_solver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=$<TARGET_FILE_DIR:aco_solver>"
            ${Python3_EXECUTABLE} -m pybind11_stubgen aco_solver --out $<TARGET_FILE_DIR:aco_solver>
    COMMENT "Generating Python stubs using Python ${Python3_VERSION}..."
    VERBATIM
  )
else()
  message(WARNING "pybind11-stubgen NOT FOUND! Stubs will not be generated.")
endif()