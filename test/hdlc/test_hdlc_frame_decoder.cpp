#include "dlms/hdlc/hdlc_codec.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::DecodeFrame;
using dlms::hdlc::DecodeFrameFromBuffer;
using dlms::hdlc::DefaultHdlcCodecLimits;
using dlms::hdlc::HdlcFrame;
using dlms::hdlc::HdlcFrameBuffer;
using dlms::hdlc::HdlcFrameKind;
using dlms::hdlc::HdlcStatus;

TEST(HdlcFrameDecoder, DecodeValidSnrm)
{
  const std::uint8_t input[] = {
    0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
  };
  HdlcFrameBuffer frame;

  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(input, sizeof(input), DefaultHdlcCodecLimits(), frame));
  EXPECT_FALSE(frame.segmented);
  EXPECT_EQ(0x91u, frame.destination.RawValue());
  EXPECT_EQ(2u, frame.destination.EncodedSize());
  EXPECT_EQ(0x64u, frame.source.RawValue());
  EXPECT_EQ(HdlcFrameKind::Unnumbered, frame.control.FrameKind());
  EXPECT_TRUE(frame.information.empty());
}

TEST(HdlcFrameDecoder, DecodeValidUaWithInformation)
{
  const std::uint8_t input[] = {
    0x7e, 0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73, 0xb4,
    0x96, 0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06,
    0x01, 0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x04, 0x00, 0x00, 0x00, 0x01, 0x5f, 0x75,
    0x7e
  };
  HdlcFrameBuffer frame;

  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(input, sizeof(input), DefaultHdlcCodecLimits(), frame));
  EXPECT_FALSE(frame.segmented);
  EXPECT_EQ(0x64u, frame.destination.RawValue());
  EXPECT_EQ(0x91u, frame.source.RawValue());
  ASSERT_EQ(21u, frame.information.size());
  EXPECT_EQ(0x7eu, frame.information[5]);
  EXPECT_EQ(0x7eu, frame.information[8]);
}

TEST(HdlcFrameDecoder, DecodeValidIFrameWithPayloadFlagByte)
{
  const std::uint8_t input[] = {
    0x7e, 0xa8, 0x10, 0x02, 0x23, 0xc9, 0x32, 0x5f,
    0x38, 0xe6, 0xe6, 0x00, 0x01, 0x7e, 0x02, 0xfe,
    0x1a, 0x7e
  };
  HdlcFrameBuffer frame;

  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(input, sizeof(input), DefaultHdlcCodecLimits(), frame));
  EXPECT_TRUE(frame.segmented);
  EXPECT_EQ(HdlcFrameKind::Information, frame.control.FrameKind());
  ASSERT_EQ(6u, frame.information.size());
  EXPECT_EQ(0x7eu, frame.information[4]);
}

TEST(HdlcFrameDecoder, DecodeFrameFromBufferCopiesInformation)
{
  const std::uint8_t input[] = {
    0x7e, 0xa8, 0x10, 0x02, 0x23, 0xc9, 0x32, 0x5f,
    0x38, 0xe6, 0xe6, 0x00, 0x01, 0x7e, 0x02, 0xfe,
    0x1a, 0x7e
  };
  std::uint8_t information[6] = {};
  HdlcFrame frame;
  std::size_t informationSize = 0u;

  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrameFromBuffer(input,
                                  sizeof(input),
                                  DefaultHdlcCodecLimits(),
                                  frame,
                                  information,
                                  sizeof(information),
                                  informationSize));
  EXPECT_EQ(6u, informationSize);
  EXPECT_EQ(information, frame.informationData);
  EXPECT_EQ(0x7eu, information[4]);
}

TEST(HdlcFrameDecoder, RejectsInvalidOpeningFlag)
{
  const std::uint8_t input[] = {
    0x00, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
  };
  HdlcFrameBuffer frame;

  EXPECT_EQ(HdlcStatus::InvalidFlag,
            DecodeFrame(input, sizeof(input), DefaultHdlcCodecLimits(), frame));
}

TEST(HdlcFrameDecoder, RejectsInvalidFormatType)
{
  const std::uint8_t input[] = {
    0x7e, 0xb0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
  };
  HdlcFrameBuffer frame;

  EXPECT_EQ(HdlcStatus::InvalidFrameFormat,
            DecodeFrame(input, sizeof(input), DefaultHdlcCodecLimits(), frame));
}

TEST(HdlcFrameDecoder, RejectsInvalidLength)
{
  const std::uint8_t input[] = {
    0x7e, 0xa0, 0x05, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
  };
  HdlcFrameBuffer frame;

  EXPECT_EQ(HdlcStatus::InvalidFrameLength,
            DecodeFrame(input, sizeof(input), DefaultHdlcCodecLimits(), frame));
}

TEST(HdlcFrameDecoder, ReportsNeedMoreDataBeforeClosingFlag)
{
  const std::uint8_t input[] = {
    0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43
  };
  HdlcFrameBuffer frame;

  EXPECT_EQ(HdlcStatus::NeedMoreData,
            DecodeFrame(input, sizeof(input), DefaultHdlcCodecLimits(), frame));
}

TEST(HdlcFrameDecoder, RejectsWrongClosingFlag)
{
  const std::uint8_t input[] = {
    0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x00
  };
  HdlcFrameBuffer frame;

  EXPECT_EQ(HdlcStatus::InvalidFlag,
            DecodeFrame(input, sizeof(input), DefaultHdlcCodecLimits(), frame));
}

TEST(HdlcFrameDecoder, RejectsInvalidHeaderChecksum)
{
  std::uint8_t input[] = {
    0x7e, 0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73, 0xb4,
    0x96, 0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06,
    0x01, 0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x04, 0x00, 0x00, 0x00, 0x01, 0x5f, 0x75,
    0x7e
  };
  input[7] ^= 0x01u;
  HdlcFrameBuffer frame;

  EXPECT_EQ(HdlcStatus::InvalidHeaderChecksum,
            DecodeFrame(input, sizeof(input), DefaultHdlcCodecLimits(), frame));
}

TEST(HdlcFrameDecoder, RejectsInvalidFrameChecksum)
{
  std::uint8_t input[] = {
    0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
  };
  input[7] ^= 0x01u;
  HdlcFrameBuffer frame;

  EXPECT_EQ(HdlcStatus::InvalidFrameChecksum,
            DecodeFrame(input, sizeof(input), DefaultHdlcCodecLimits(), frame));
}

TEST(HdlcFrameDecoder, RejectsSmallInformationBuffer)
{
  const std::uint8_t input[] = {
    0x7e, 0xa8, 0x10, 0x02, 0x23, 0xc9, 0x32, 0x5f,
    0x38, 0xe6, 0xe6, 0x00, 0x01, 0x7e, 0x02, 0xfe,
    0x1a, 0x7e
  };
  std::uint8_t information[4] = {};
  HdlcFrame frame;
  std::size_t informationSize = 0u;

  EXPECT_EQ(HdlcStatus::OutputBufferTooSmall,
            DecodeFrameFromBuffer(input,
                                  sizeof(input),
                                  DefaultHdlcCodecLimits(),
                                  frame,
                                  information,
                                  sizeof(information),
                                  informationSize));
}

} // namespace
