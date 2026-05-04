cmake_minimum_required(VERSION 3.16)

project(dlms_hdlc
  VERSION 0.1.0
  LANGUAGES C CXX)

option(DLMS_BUILD_TESTS      "Build GoogleTest tests"                ON)
option(DLMS_BUILD_C_API      "Build stable C ABI wrapper"            ON)
option(DLMS_USE_SYSTEM_GTEST "Use system-installed GoogleTest"       OFF)
option(DLMS_BUILD_GO         "Build Go implementation (godlms_hdlc)" OFF)

add_library(dlms_hdlc
  src/hdlc/hdlc_address.cpp
  src/hdlc/hdlc_control.cpp
  src/hdlc/hdlc_crc.cpp
  src/hdlc/hdlc_codec.cpp
  src/hdlc/hdlc_stream_decoder.cpp
  src/hdlc/hdlc_segmentation.cpp
  src/hdlc/hdlc_session.cpp)

target_compile_features(dlms_hdlc PUBLIC cxx_std_11)

target_include_directories(dlms_hdlc
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(DLMS_BUILD_C_API)
  target_sources(dlms_hdlc
    PRIVATE
      src/hdlc/hdlc_c_api.cpp)
endif()

if(DLMS_BUILD_TESTS OR DLMS_BUILD_GO)
  include(CTest)
endif()

if(DLMS_BUILD_TESTS)
  add_subdirectory(test)
endif()

if(DLMS_BUILD_GO)
  add_subdirectory(golang)
endif()
