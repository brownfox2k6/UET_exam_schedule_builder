cmake_minimum_required(VERSION 3.24)
project(aco_solver CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenMP REQUIRED)
find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)

include(FetchContent)
find_package(nanobind CONFIG QUIET)
if(NOT nanobind_FOUND)
  FetchContent_Declare(
    nanobind
    GIT_REPOSITORY https://github.com/wjakob/nanobind.git
    GIT_TAG        v3.0.0
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(nanobind)
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

nanobind_add_module(aco_solver ${SOURCES})
set_target_properties(nanobind-static PROPERTIES
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:nanobind-static,INTERFACE_INCLUDE_DIRECTORIES>
)

target_include_directories(aco_solver PRIVATE include)
target_compile_features(aco_solver PRIVATE cxx_std_23)

if(MSVC)
  target_compile_options(aco_solver PRIVATE /W4)
else()
  target_compile_options(aco_solver PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wunused
  )
endif()

set_target_properties(aco_solver PROPERTIES COMPILE_WARNING_AS_ERROR ON)

if(OpenMP_CXX_FOUND)
  target_link_libraries(aco_solver PRIVATE OpenMP::OpenMP_CXX)
endif()

set_target_properties(aco_solver PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

execute_process(
  COMMAND "${Python3_EXECUTABLE}" -c "import nanobind.stubgen"
  RESULT_VARIABLE STUBGEN_IMPORT_RESULT
  OUTPUT_QUIET
  ERROR_QUIET
)

if(STUBGEN_IMPORT_RESULT EQUAL 0)
  add_custom_command(TARGET aco_solver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=$<TARGET_FILE_DIR:aco_solver>"
            ${Python3_EXECUTABLE} -m nanobind.stubgen -m aco_solver -o $<TARGET_FILE_DIR:aco_solver>
    COMMENT "Generating Python stubs using nanobind.stubgen..."
    VERBATIM
  )
else()
  message(STATUS "nanobind.stubgen is not available; Python stubs will not be generated.")
endif()