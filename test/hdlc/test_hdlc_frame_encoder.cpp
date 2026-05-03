#include "dlms/hdlc/hdlc_codec.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::DefaultHdlcCodecLimits;
using dlms::hdlc::DlmsHdlcAddress;
using dlms::hdlc::EncodeFrame;
using dlms::hdlc::EncodeFrameToBuffer;
using dlms::hdlc::HdlcControl;
using dlms::hdlc::HdlcFrame;
using dlms::hdlc::HdlcStatus;

HdlcFrame MakeFrame(std::uint8_t controlValue)
{
  HdlcFrame frame;
  frame.segmented = false;
  EXPECT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeServerAddress(0x01u,
                                               0x11u,
                                               frame.destination));
  EXPECT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeClientAddress(0x64u, frame.source));
  EXPECT_EQ(HdlcStatus::Ok, HdlcControl::Decode(controlValue, frame.control));
  frame.informationData = 0;
  frame.informationSize = 0u;
  return frame;
}

std::vector<std::uint8_t> EncodeOrEmpty(const HdlcFrame& frame)
{
  std::vector<std::uint8_t> output;
  EXPECT_EQ(HdlcStatus::Ok,
            EncodeFrame(frame, DefaultHdlcCodecLimits(), output));
  return output;
}

TEST(HdlcFrameEncoder, EncodeSnrm)
{
  const HdlcFrame frame = MakeFrame(0x93u);

  const std::vector<std::uint8_t> output = EncodeOrEmpty(frame);

  const std::uint8_t expected[] = {
    0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
  };
  ASSERT_EQ(sizeof(expected), output.size());
  EXPECT_TRUE(std::equal(output.begin(), output.end(), expected));
}

TEST(HdlcFrameEncoder, EncodeDisc)
{
  const HdlcFrame frame = MakeFrame(0x53u);

  const std::vector<std::uint8_t> output = EncodeOrEmpty(frame);

  const std::uint8_t expected[] = {
    0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x53, 0xe8, 0x85, 0x7e
  };
  ASSERT_EQ(sizeof(expected), output.size());
  EXPECT_TRUE(std::equal(output.begin(), output.end(), expected));
}

TEST(HdlcFrameEncoder, EncodeReceiveReady)
{
  const HdlcFrame frame = MakeFrame(0x11u);

  const std::vector<std::uint8_t> output = EncodeOrEmpty(frame);

  const std::uint8_t expected[] = {
    0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x11, 0xfe, 0xe4, 0x7e
  };
  ASSERT_EQ(sizeof(expected), output.size());
  EXPECT_TRUE(std::equal(output.begin(), output.end(), expected));
}

TEST(HdlcFrameEncoder, EncodeUaWithInformation)
{
  HdlcFrame frame = MakeFrame(0x73u);
  EXPECT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeClientAddress(0x64u, frame.destination));
  EXPECT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeServerAddress(0x01u, 0x11u, frame.source));
  const std::uint8_t information[] = {
    0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06, 0x01,
    0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01, 0x08,
    0x04, 0x00, 0x00, 0x00, 0x01
  };
  frame.informationData = information;
  frame.informationSize = sizeof(information);

  const std::vector<std::uint8_t> output = EncodeOrEmpty(frame);

  const std::uint8_t expected[] = {
    0x7e, 0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73, 0xb4,
    0x96, 0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06,
    0x01, 0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x04, 0x00, 0x00, 0x00, 0x01, 0x5f, 0x75,
    0x7e
  };
  ASSERT_EQ(sizeof(expected), output.size());
  EXPECT_TRUE(std::equal(output.begin(), output.end(), expected));
}

TEST(HdlcFrameEncoder, EncodeIFrameWithSegmentationBitAndPayloadFlagByte)
{
  HdlcFrame frame = MakeFrame(0x32u);
  frame.segmented = true;
  const std::uint8_t information[] = {
    0xe6, 0xe6, 0x00, 0x01, 0x7e, 0x02
  };
  frame.informationData = information;
  frame.informationSize = sizeof(information);

  const std::vector<std::uint8_t> output = EncodeOrEmpty(frame);

  ASSERT_EQ(18u, output.size());
  EXPECT_EQ(0x7eu, output[0]);
  EXPECT_EQ(0xa8u, output[1]);
  EXPECT_EQ(0x10u, output[2]);
  EXPECT_EQ(0x7eu, output[13]);
  EXPECT_EQ(0x7eu, output[17]);
}

TEST(HdlcFrameEncoder, EncodeFrameToBufferReportsSmallOutput)
{
  const HdlcFrame frame = MakeFrame(0x93u);
  std::uint8_t output[4] = {};
  std::size_t written = 99u;

  EXPECT_EQ(HdlcStatus::OutputBufferTooSmall,
            EncodeFrameToBuffer(frame,
                                DefaultHdlcCodecLimits(),
                                output,
                                sizeof(output),
                                written));
  EXPECT_EQ(0u, written);
}

TEST(HdlcFrameEncoder, RejectsNullInformationWithNonZeroSize)
{
  HdlcFrame frame = MakeFrame(0x32u);
  frame.informationData = 0;
  frame.informationSize = 1u;
  std::uint8_t output[32] = {};
  std::size_t written = 0u;

  EXPECT_EQ(HdlcStatus::InvalidArgument,
            EncodeFrameToBuffer(frame,
                                DefaultHdlcCodecLimits(),
                                output,
                                sizeof(output),
                                written));
}

TEST(HdlcFrameEncoder, RejectsInformationFieldTooLarge)
{
  HdlcFrame frame = MakeFrame(0x32u);
  const std::uint8_t information[] = {0x01, 0x02};
  frame.informationData = information;
  frame.informationSize = sizeof(information);
  dlms::hdlc::HdlcCodecLimits limits = DefaultHdlcCodecLimits();
  limits.maximumInformationFieldSize = 1u;
  std::uint8_t output[32] = {};
  std::size_t written = 0u;

  EXPECT_EQ(HdlcStatus::InformationFieldTooLarge,
            EncodeFrameToBuffer(frame, limits, output, sizeof(output), written));
}

TEST(HdlcFrameEncoder, RejectsFrameTooLarge)
{
  const HdlcFrame frame = MakeFrame(0x93u);
  dlms::hdlc::HdlcCodecLimits limits = DefaultHdlcCodecLimits();
  limits.maximumFrameSize = 9u;
  std::uint8_t output[32] = {};
  std::size_t written = 0u;

  EXPECT_EQ(HdlcStatus::FrameTooLarge,
            EncodeFrameToBuffer(frame, limits, output, sizeof(output), written));
}

} // namespace
