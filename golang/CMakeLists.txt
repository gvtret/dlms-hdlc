# Go implementation of the DLMS/COSEM HDLC codec.
#
# Builds the shared library  libgodlms_hdlc.{so,dylib,dll}  and registers
# Go unit tests with CTest so they run alongside the C++ tests.
#
# Requires:
#   - Go 1.21+ in PATH
#   - CGO_ENABLED=1 (default) for the shared-library target
#
# Targets produced:
#   godlms_hdlc_build   – custom target that drives `go build -buildmode=c-shared`
#   godlms_hdlc         – IMPORTED SHARED library, usable with target_link_libraries
#
# CTest tests registered:
#   godlms_hdlc_go_tests – runs `go test ./hdlc/...`

cmake_minimum_required(VERSION 3.16)

# ---------------------------------------------------------------------------
# Locate Go
# ---------------------------------------------------------------------------
find_program(GO_EXECUTABLE go
  DOC "Go compiler executable (go)")

if(NOT GO_EXECUTABLE)
  message(FATAL_ERROR
    "Go compiler not found. Install Go 1.21+ or set GO_EXECUTABLE.\n"
    "Alternatively, disable this subproject with -DDLMS_BUILD_GO=OFF")
endif()

execute_process(
  COMMAND "${GO_EXECUTABLE}" version
  OUTPUT_VARIABLE GO_VERSION_OUTPUT
  OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Found Go: ${GO_EXECUTABLE} (${GO_VERSION_OUTPUT})")

# ---------------------------------------------------------------------------
# Shared-library output paths (platform-aware)
# ---------------------------------------------------------------------------
# CMAKE_SHARED_LIBRARY_PREFIX : "lib" on Linux/macOS, "" on Windows
# CMAKE_SHARED_LIBRARY_SUFFIX : ".so"/".dylib"/".dll"
set(_GO_LIB_BASE    "godlms_hdlc")
set(_GO_LIB_NAME    "${CMAKE_SHARED_LIBRARY_PREFIX}${_GO_LIB_BASE}${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(_GO_LIB_OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/${_GO_LIB_NAME}")

# CGo generates <prefix><base>.h alongside the shared library.
set(_GO_HDR_OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}${_GO_LIB_BASE}.h")

# On Windows, CGo also produces an import library (<base>.lib or <base>.dll.a).
if(WIN32)
  set(_GO_IMPLIB_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_GO_LIB_BASE}.lib")
elseif(MINGW)
  set(_GO_IMPLIB_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/lib${_GO_LIB_BASE}.dll.a")
endif()

# ---------------------------------------------------------------------------
# Source file list for dependency tracking
# ---------------------------------------------------------------------------
file(GLOB_RECURSE _GO_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/hdlc/*.go"
  "${CMAKE_CURRENT_SOURCE_DIR}/cabi/*.go"
  "${CMAKE_CURRENT_SOURCE_DIR}/cabi/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/go.mod")

# ---------------------------------------------------------------------------
# Custom command: invoke `go build -buildmode=c-shared`
# ---------------------------------------------------------------------------
add_custom_command(
  OUTPUT
    "${_GO_LIB_OUTPUT}"
    "${_GO_HDR_OUTPUT}"
  COMMAND
    "${CMAKE_COMMAND}" -E env CGO_ENABLED=1
    "${GO_EXECUTABLE}" build
      -buildmode=c-shared
      -o "${_GO_LIB_OUTPUT}"
      ./cabi/
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS ${_GO_SOURCES}
  COMMENT "Building Go HDLC shared library: ${_GO_LIB_NAME}"
  VERBATIM)

# Custom target so `cmake --build` always drives the Go build.
add_custom_target(godlms_hdlc_build ALL
  DEPENDS "${_GO_LIB_OUTPUT}")

# ---------------------------------------------------------------------------
# Imported SHARED target – lets downstream CMake code do:
#   target_link_libraries(my_app PRIVATE godlms_hdlc)
# ---------------------------------------------------------------------------
add_library(godlms_hdlc SHARED IMPORTED GLOBAL)
add_dependencies(godlms_hdlc godlms_hdlc_build)

set_target_properties(godlms_hdlc PROPERTIES
  IMPORTED_LOCATION             "${_GO_LIB_OUTPUT}"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}")

if(DEFINED _GO_IMPLIB_OUTPUT)
  set_target_properties(godlms_hdlc PROPERTIES
    IMPORTED_IMPLIB "${_GO_IMPLIB_OUTPUT}")
endif()

# ---------------------------------------------------------------------------
# CTest: `go test ./hdlc/...`
#
# The hdlc package is pure Go (no CGo), so CGO_ENABLED=0 is safe and avoids
# requiring a C toolchain when only running tests.
# ---------------------------------------------------------------------------
add_test(
  NAME    godlms_hdlc_go_tests
  COMMAND "${GO_EXECUTABLE}" test -v ./hdlc/...
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

set_tests_properties(godlms_hdlc_go_tests PROPERTIES
  ENVIRONMENT "CGO_ENABLED=0"
  LABELS      "go")
