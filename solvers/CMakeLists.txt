cmake_minimum_required(VERSION 3.24)
project(cetp_solvers CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python 3.11 REQUIRED COMPONENTS Interpreter Development.Module)

set(NANOBIND_LOCAL_PYTHON_PATH "${CMAKE_BINARY_DIR}/_python")
set(NANOBIND_PYTHONPATH "${NANOBIND_LOCAL_PYTHON_PATH}")
if(DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
  if(WIN32)
    string(APPEND NANOBIND_PYTHONPATH ";$ENV{PYTHONPATH}")
  else()
    string(APPEND NANOBIND_PYTHONPATH ":$ENV{PYTHONPATH}")
  endif()
endif()
set(NANOBIND_PYTHON_COMMAND
  "${CMAKE_COMMAND}" -E env "PYTHONPATH=${NANOBIND_PYTHONPATH}" "${Python_EXECUTABLE}"
)

message(STATUS "Installing the latest nanobind into ${NANOBIND_LOCAL_PYTHON_PATH}")
execute_process(
  COMMAND "${Python_EXECUTABLE}" -m pip install --disable-pip-version-check --upgrade
          --target "${NANOBIND_LOCAL_PYTHON_PATH}" nanobind
  RESULT_VARIABLE NANOBIND_INSTALL_RESULT
  OUTPUT_VARIABLE NANOBIND_INSTALL_OUTPUT
  ERROR_VARIABLE NANOBIND_INSTALL_ERROR
)
if(NOT NANOBIND_INSTALL_RESULT EQUAL 0)
  message(FATAL_ERROR
    "Could not install the latest nanobind into the build directory.\n"
    "Command: ${Python_EXECUTABLE} -m pip install --upgrade "
    "--target ${NANOBIND_LOCAL_PYTHON_PATH} nanobind\n"
    "${NANOBIND_INSTALL_OUTPUT}${NANOBIND_INSTALL_ERROR}"
  )
endif()

execute_process(
  COMMAND ${NANOBIND_PYTHON_COMMAND} -m nanobind --cmake_dir
  OUTPUT_VARIABLE NANOBIND_CMAKE_DIR
  RESULT_VARIABLE NANOBIND_CMAKE_DIR_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT NANOBIND_CMAKE_DIR_RESULT EQUAL 0 OR NOT IS_DIRECTORY "${NANOBIND_CMAKE_DIR}")
  message(FATAL_ERROR
    "The latest nanobind was installed but its CMake package directory could not be found "
    "using ${Python_EXECUTABLE}."
  )
endif()

list(PREPEND CMAKE_PREFIX_PATH "${NANOBIND_CMAKE_DIR}")
find_package(nanobind CONFIG REQUIRED)

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

find_package(OpenMP REQUIRED)
target_link_libraries(cetp_solvers PRIVATE OpenMP::OpenMP_CXX)

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
  COMMAND ${NANOBIND_PYTHON_COMMAND} -c "import nanobind.stubgen"
  RESULT_VARIABLE STUBGEN_IMPORT_RESULT
  OUTPUT_QUIET
  ERROR_QUIET
)
if(STUBGEN_IMPORT_RESULT EQUAL 0)
  add_custom_command(TARGET cetp_solvers POST_BUILD
    COMMAND ${NANOBIND_PYTHON_COMMAND} -m nanobind.stubgen
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