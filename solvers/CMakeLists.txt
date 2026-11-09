cmake_minimum_required(VERSION 3.24)
project(cetp_solvers CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python 3.11 REQUIRED COMPONENTS Interpreter Development.Module)

execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_VARIABLE NANOBIND_CMAKE_DIR
  RESULT_VARIABLE NANOBIND_CMAKE_DIR_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_VARIABLE NANOBIND_CMAKE_DIR_ERROR
)
if(NOT NANOBIND_CMAKE_DIR_RESULT EQUAL 0 OR NOT IS_DIRECTORY "${NANOBIND_CMAKE_DIR}")
  message(FATAL_ERROR
    "nanobind is not installed for the Python interpreter selected by CMake:\n"
    "  ${Python_EXECUTABLE}\n"
    "Install the project's Python build dependencies before configuring, for example:\n"
    "  ${Python_EXECUTABLE} -m pip install -r "
    "${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt\n"
    "${NANOBIND_CMAKE_DIR_ERROR}"
  )
endif()

find_package(nanobind CONFIG REQUIRED
  PATHS "${NANOBIND_CMAKE_DIR}"
  NO_DEFAULT_PATH
)

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

nanobind_add_module(cetp_solvers ${SOURCES})

target_include_directories(cetp_solvers PRIVATE include)
target_compile_features(cetp_solvers PRIVATE cxx_std_23)

if(TARGET nanobind-static)
  get_target_property(NANOBIND_INCLUDE_DIRS nanobind-static INTERFACE_INCLUDE_DIRECTORIES)
  if(NANOBIND_INCLUDE_DIRS)
    target_include_directories(cetp_solvers SYSTEM PRIVATE ${NANOBIND_INCLUDE_DIRS})
  endif()
endif()

find_package(OpenMP QUIET COMPONENTS CXX)
if(NOT OpenMP_CXX_FOUND)
  if(APPLE)
    set(OPENMP_INSTALL_HINT
      "Install the OpenMP runtime for AppleClang (for example, 'brew install libomp'), "
      "then configure again."
    )
  elseif(MSVC)
    set(OPENMP_INSTALL_HINT
      "Use a compiler/toolset with OpenMP support enabled, then configure again."
    )
  else()
    set(OPENMP_INSTALL_HINT
      "Install the OpenMP development/runtime package for your compiler, then configure again."
    )
  endif()
  message(FATAL_ERROR
    "OpenMP for C++ was not found for compiler '${CMAKE_CXX_COMPILER_ID}'.\n"
    "${OPENMP_INSTALL_HINT}"
  )
endif()
target_link_libraries(cetp_solvers PRIVATE OpenMP::OpenMP_CXX)

find_package(ortools CONFIG REQUIRED)
# Some Homebrew OR-Tools packages export optional MathOpt object targets without
# exporting the targets themselves. The shared OR-Tools library already contains
# the solver implementation, so discard only the broken optional object list.
if(NOT TARGET ortools_math_opt_core)
  set_property(TARGET ortools::ortools PROPERTY INTERFACE_SOURCES "")
  if(TARGET ortools::ortools_math_opt)
    set_property(TARGET ortools::ortools_math_opt PROPERTY INTERFACE_SOURCES "")
  endif()
  if(TARGET ortools::ortools_math_opt_constraints)
    set_property(TARGET ortools::ortools_math_opt_constraints PROPERTY INTERFACE_SOURCES "")
  endif()
endif()
target_link_libraries(cetp_solvers PRIVATE ortools::ortools)

if(MSVC)
  target_compile_options(cetp_solvers PRIVATE /W4)
else()
  target_compile_options(cetp_solvers PRIVATE
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

set_target_properties(cetp_solvers PROPERTIES COMPILE_WARNING_AS_ERROR ON)

set_target_properties(cetp_solvers PROPERTIES
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
  add_custom_command(TARGET cetp_solvers POST_BUILD
    COMMAND "${Python_EXECUTABLE}" -m nanobind.stubgen
            --import "$<TARGET_FILE_DIR:cetp_solvers>"
            --module cetp_solvers
            --output-file "${CMAKE_CURRENT_SOURCE_DIR}/cetp_solvers.pyi"
    COMMENT "Generating Python stubs with nanobind.stubgen"
    VERBATIM
  )
else()
  message(STATUS "nanobind.stubgen is unavailable; Python stubs will not be generated")
endif()

# Direct CMake builds place the module beside solvers/__init__.py via the output
# properties, while install rules support standard CMake installs.
install(TARGETS cetp_solvers
  LIBRARY DESTINATION solvers
  RUNTIME DESTINATION solvers
  ARCHIVE DESTINATION solvers
)
