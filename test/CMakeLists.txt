if(DLMS_USE_SYSTEM_GTEST)
  find_package(GTest REQUIRED)
else()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(dlms_hdlc_tests
  hdlc/test_hdlc_c_header.c
  hdlc/test_hdlc_address.cpp
  hdlc/test_hdlc_c_api.cpp
  hdlc/test_hdlc_control.cpp
  hdlc/test_hdlc_crc.cpp
  hdlc/test_hdlc_error.cpp
  hdlc/test_hdlc_frame_decoder.cpp
  hdlc/test_hdlc_frame_encoder.cpp
  hdlc/test_hdlc_real_vectors.cpp
  hdlc/test_hdlc_segmentation.cpp
  hdlc/test_hdlc_session.cpp
  hdlc/test_hdlc_stream_decoder.cpp)

target_link_libraries(dlms_hdlc_tests
  PRIVATE
    dlms_hdlc
    GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(dlms_hdlc_tests)
