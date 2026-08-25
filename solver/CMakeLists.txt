cmake_minimum_required(VERSION 3.24)
project(aco_solver CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)

# Prefer nanobind installed in the selected Python environment. FetchContent
# keeps direct CMake builds working without a separate nanobind installation.
execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_VARIABLE NANOBIND_CMAKE_DIR
  RESULT_VARIABLE NANOBIND_CMAKE_DIR_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NANOBIND_CMAKE_DIR_RESULT EQUAL 0 AND IS_DIRECTORY "${NANOBIND_CMAKE_DIR}")
  list(PREPEND CMAKE_PREFIX_PATH "${NANOBIND_CMAKE_DIR}")
endif()

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

# AppleClang does not search Homebrew's libomp prefix by default.
if(APPLE AND NOT OpenMP_ROOT)
  find_program(BREW_EXECUTABLE brew)
  if(BREW_EXECUTABLE)
    execute_process(
      COMMAND "${BREW_EXECUTABLE}" --prefix libomp
      OUTPUT_VARIABLE HOMEBREW_LIBOMP_ROOT
      RESULT_VARIABLE HOMEBREW_LIBOMP_RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(HOMEBREW_LIBOMP_RESULT EQUAL 0 AND IS_DIRECTORY "${HOMEBREW_LIBOMP_ROOT}")
      set(OpenMP_ROOT "${HOMEBREW_LIBOMP_ROOT}")
    endif()
  endif()
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

nanobind_add_module(aco_solver ${SOURCES})

target_include_directories(aco_solver PRIVATE include)
target_compile_features(aco_solver PRIVATE cxx_std_23)

if(TARGET nanobind-static)
  get_target_property(NANOBIND_INCLUDE_DIRS nanobind-static INTERFACE_INCLUDE_DIRECTORIES)
  if(NANOBIND_INCLUDE_DIRS)
    target_include_directories(aco_solver SYSTEM PRIVATE ${NANOBIND_INCLUDE_DIRS})
  endif()
endif()

find_package(OpenMP REQUIRED)
target_link_libraries(aco_solver PRIVATE OpenMP::OpenMP_CXX)

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
    -Wunused
  )
endif()

set_target_properties(aco_solver PROPERTIES COMPILE_WARNING_AS_ERROR ON)

set_target_properties(aco_solver PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

execute_process(
  COMMAND "${Python_EXECUTABLE}" -c "import nanobind.stubgen"
  RESULT_VARIABLE STUBGEN_IMPORT_RESULT
  OUTPUT_QUIET
  ERROR_QUIET
)
if(STUBGEN_IMPORT_RESULT EQUAL 0)
  add_custom_command(TARGET aco_solver POST_BUILD
    COMMAND "${Python_EXECUTABLE}" -m nanobind.stubgen
            --import "$<TARGET_FILE_DIR:aco_solver>"
            --module aco_solver
            --output-file "${CMAKE_CURRENT_SOURCE_DIR}/aco_solver.pyi"
    COMMENT "Generating Python stubs with nanobind.stubgen"
    VERBATIM
  )
else()
  message(STATUS "nanobind.stubgen is unavailable; Python stubs will not be generated")
endif()

# Direct CMake builds place the module beside solver/__init__.py via the output
# properties, while install rules support standard CMake installs.
install(TARGETS aco_solver
  LIBRARY DESTINATION solver
  RUNTIME DESTINATION solver
  ARCHIVE DESTINATION solver
)