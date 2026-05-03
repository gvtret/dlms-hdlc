#include "dlms/hdlc/hdlc_c_api.h"

#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

namespace {

dlms_hdlc_frame_t MakeSnrmFrame()
{
  dlms_hdlc_frame_t frame;
  frame.segmented = 0u;
  frame.destination_address_raw = 0x01u;
  frame.destination_address_size = 1u;
  frame.source_address_raw = 0x10u;
  frame.source_address_size = 1u;
  frame.control = 0x93u;
  frame.information_data = 0;
  frame.information_size = 0u;
  return frame;
}

TEST(HdlcCApi, StatusValuesMatchStableAbi)
{
  EXPECT_EQ(0, DLMS_HDLC_STATUS_OK);
  EXPECT_EQ(1, DLMS_HDLC_STATUS_NEED_MORE_DATA);
  EXPECT_EQ(2, DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL);
  EXPECT_EQ(3, DLMS_HDLC_STATUS_INVALID_ARGUMENT);
  EXPECT_EQ(20, DLMS_HDLC_STATUS_INTERNAL_ERROR);
}

TEST(HdlcCApi, EncodeFrame)
{
  const dlms_hdlc_frame_t frame = MakeSnrmFrame();
  std::uint8_t output[32] = {};
  std::size_t writtenSize = 0u;

  ASSERT_EQ(DLMS_HDLC_STATUS_OK,
            dlms_hdlc_encode_frame(&frame,
                                   0,
                                   output,
                                   sizeof(output),
                                   &writtenSize));
  EXPECT_EQ(9u, writtenSize);
  EXPECT_EQ(0x7eu, output[0]);
  EXPECT_EQ(0x7eu, output[writtenSize - 1u]);
}

TEST(HdlcCApi, DecodeFrame)
{
  const dlms_hdlc_frame_t frame = MakeSnrmFrame();
  std::uint8_t encoded[32] = {};
  std::size_t writtenSize = 0u;
  ASSERT_EQ(DLMS_HDLC_STATUS_OK,
            dlms_hdlc_encode_frame(&frame,
                                   0,
                                   encoded,
                                   sizeof(encoded),
                                   &writtenSize));

  dlms_hdlc_frame_t decoded;
  std::uint8_t information[8] = {};
  std::size_t informationSize = 0u;
  ASSERT_EQ(DLMS_HDLC_STATUS_OK,
            dlms_hdlc_decode_frame(encoded,
                                   writtenSize,
                                   0,
                                   &decoded,
                                   information,
                                   sizeof(information),
                                   &informationSize));
  EXPECT_EQ(0u, decoded.segmented);
  EXPECT_EQ(0x01u, decoded.destination_address_raw);
  EXPECT_EQ(1u, decoded.destination_address_size);
  EXPECT_EQ(0x10u, decoded.source_address_raw);
  EXPECT_EQ(1u, decoded.source_address_size);
  EXPECT_EQ(0x93u, decoded.control);
  EXPECT_EQ(0u, informationSize);
}

TEST(HdlcCApi, ReportsSmallOutputBuffer)
{
  const dlms_hdlc_frame_t frame = MakeSnrmFrame();
  std::uint8_t output[4] = {};
  std::size_t writtenSize = 99u;

  EXPECT_EQ(DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL,
            dlms_hdlc_encode_frame(&frame,
                                   0,
                                   output,
                                   sizeof(output),
                                   &writtenSize));
  EXPECT_EQ(0u, writtenSize);
}

TEST(HdlcCApi, ValidatesNullArguments)
{
  const dlms_hdlc_frame_t frame = MakeSnrmFrame();
  std::uint8_t output[32] = {};
  std::size_t writtenSize = 0u;

  EXPECT_EQ(DLMS_HDLC_STATUS_INVALID_ARGUMENT,
            dlms_hdlc_encode_frame(0, 0, output, sizeof(output), &writtenSize));
  EXPECT_EQ(DLMS_HDLC_STATUS_INVALID_ARGUMENT,
            dlms_hdlc_encode_frame(&frame, 0, 0, sizeof(output), &writtenSize));
  EXPECT_EQ(DLMS_HDLC_STATUS_INVALID_ARGUMENT,
            dlms_hdlc_encode_frame(&frame, 0, output, sizeof(output), 0));
  EXPECT_EQ(DLMS_HDLC_STATUS_INVALID_ARGUMENT,
            dlms_hdlc_decode_frame(0, 0, 0, 0, 0, 0, 0));
}

TEST(HdlcCApi, StreamDecoderLifecycle)
{
  dlms_hdlc_stream_decoder_t* decoder = 0;

  ASSERT_EQ(DLMS_HDLC_STATUS_OK,
            dlms_hdlc_stream_decoder_create(0, &decoder));
  ASSERT_NE(static_cast<dlms_hdlc_stream_decoder_t*>(0), decoder);
  dlms_hdlc_stream_decoder_reset(decoder);
  dlms_hdlc_stream_decoder_destroy(decoder);

  EXPECT_EQ(DLMS_HDLC_STATUS_INVALID_ARGUMENT,
            dlms_hdlc_stream_decoder_create(0, 0));
  dlms_hdlc_stream_decoder_reset(0);
  dlms_hdlc_stream_decoder_destroy(0);
}

TEST(HdlcCApi, ReassemblerLifecycle)
{
  dlms_hdlc_reassembler_t* reassembler = 0;

  ASSERT_EQ(DLMS_HDLC_STATUS_OK,
            dlms_hdlc_reassembler_create(0, &reassembler));
  ASSERT_NE(static_cast<dlms_hdlc_reassembler_t*>(0), reassembler);
  dlms_hdlc_reassembler_reset(reassembler);
  dlms_hdlc_reassembler_destroy(reassembler);

  EXPECT_EQ(DLMS_HDLC_STATUS_INVALID_ARGUMENT,
            dlms_hdlc_reassembler_create(0, 0));
  dlms_hdlc_reassembler_reset(0);
  dlms_hdlc_reassembler_destroy(0);
}

TEST(HdlcCApi, InvalidInputsDoNotCrash)
{
  dlms_hdlc_frame_t frame = MakeSnrmFrame();
  frame.destination_address_size = 3u;
  std::uint8_t output[32] = {};
  std::size_t writtenSize = 0u;

  EXPECT_EQ(DLMS_HDLC_STATUS_UNSUPPORTED_ADDRESS,
            dlms_hdlc_encode_frame(&frame,
                                   0,
                                   output,
                                   sizeof(output),
                                   &writtenSize));
}

} // namespace
