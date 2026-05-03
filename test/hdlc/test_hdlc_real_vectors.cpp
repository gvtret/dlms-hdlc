#include "dlms/hdlc/hdlc_codec.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::DecodeFrame;
using dlms::hdlc::DefaultHdlcCodecLimits;
using dlms::hdlc::EncodeFrame;
using dlms::hdlc::HdlcFrame;
using dlms::hdlc::HdlcFrameBuffer;
using dlms::hdlc::HdlcFrameKind;
using dlms::hdlc::HdlcStatus;

struct HdlcRealVector
{
  const char* name;
  const std::uint8_t* data;
  std::size_t size;
  bool segmented;
  HdlcFrameKind frameKind;
  std::uint32_t destinationAddressRaw;
  std::uint32_t sourceAddressRaw;
  std::size_t informationSize;
};

const std::uint8_t kSnrmRequest[] = {
  0x7e, 0xa0, 0x08, 0x02, 0x21, 0x61, 0x93, 0x63, 0x97, 0x7e
};

const std::uint8_t kUaResponse[] = {
  0x7e, 0xa0, 0x21, 0x61, 0x02, 0x21, 0x73, 0x88,
  0x57, 0x81, 0x80, 0x14, 0x05, 0x02, 0x00, 0x80,
  0x06, 0x02, 0x00, 0x80, 0x07, 0x04, 0x00, 0x00,
  0x00, 0x01, 0x08, 0x04, 0x00, 0x00, 0x00, 0x01,
  0xce, 0x6a, 0x7e
};

const std::uint8_t kInformationWithLlcPayload[] = {
  0x7e, 0xa0, 0x51, 0x02, 0x21, 0x61, 0x10, 0xf6,
  0x05, 0xe6, 0xe6, 0x00, 0x60, 0x42, 0x80, 0x02,
  0x02, 0x84, 0xa1, 0x09, 0x06, 0x07, 0x60, 0x85,
  0x74, 0x05, 0x08, 0x01, 0x01, 0x8a, 0x02, 0x07,
  0x80, 0x8b, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08,
  0x02, 0x02, 0xac, 0x12, 0x80, 0x10, 0x16, 0x1e,
  0x69, 0x35, 0x35, 0x25, 0x6c, 0x25, 0x42, 0x52,
  0x6b, 0x48, 0x26, 0x72, 0x17, 0x42, 0xbe, 0x10,
  0x04, 0x0e, 0x01, 0x00, 0x00, 0x00, 0x06, 0x5f,
  0x1f, 0x04, 0x00, 0x62, 0x1e, 0x5d, 0x02, 0x00,
  0x1b, 0x89, 0x7e
};

const std::uint8_t kReceiveReady[] = {
  0x7e, 0xa0, 0x08, 0x02, 0x21, 0x61, 0xd1, 0x75, 0xf6, 0x7e
};

const std::uint8_t kDiscRequest[] = {
  0x7e, 0xa0, 0x08, 0x02, 0x21, 0x61, 0x53, 0x6f, 0x51, 0x7e
};

const std::uint8_t kHistoricalSnrmRequest[] = {
  0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
};

const std::uint8_t kHistoricalUaResponse[] = {
  0x7e, 0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73, 0xb4,
  0x96, 0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06,
  0x01, 0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01,
  0x08, 0x04, 0x00, 0x00, 0x00, 0x01, 0x5f, 0x75,
  0x7e
};

const std::uint8_t kHistoricalDiscRequest[] = {
  0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x53, 0xe8, 0x85, 0x7e
};

const std::uint8_t kHistoricalReceiveReady[] = {
  0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x11, 0xfe, 0xe4, 0x7e
};

const std::uint8_t kHistoricalInformationWithLlcPayload[] = {
  0x7e, 0xa0, 0x1a, 0x02, 0x23, 0xc9, 0x32, 0xaf,
  0x55, 0xe6, 0xe6, 0x00, 0xc0, 0x01, 0x40, 0x00,
  0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0xff, 0x02,
  0x00, 0xea, 0xdd, 0x7e
};

std::vector<std::uint8_t> EncodeDecodedFrame(const HdlcFrameBuffer& frame)
{
  HdlcFrame encodeFrame;
  encodeFrame.segmented = frame.segmented;
  encodeFrame.destination = frame.destination;
  encodeFrame.source = frame.source;
  encodeFrame.control = frame.control;
  encodeFrame.informationData =
    frame.information.empty() ? 0 : &frame.information[0];
  encodeFrame.informationSize = frame.information.size();

  std::vector<std::uint8_t> encoded;
  EXPECT_EQ(HdlcStatus::Ok,
            EncodeFrame(encodeFrame, DefaultHdlcCodecLimits(), encoded));
  return encoded;
}

class HdlcRealVectorTest
  : public testing::TestWithParam<HdlcRealVector>
{
};

TEST_P(HdlcRealVectorTest, DecodeAndEncodeKnownDlmsHdlcVector)
{
  const HdlcRealVector vector = GetParam();
  HdlcFrameBuffer frame;

  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(vector.data,
                        vector.size,
                        DefaultHdlcCodecLimits(),
                        frame));

  EXPECT_EQ(vector.segmented, frame.segmented);
  EXPECT_EQ(vector.frameKind, frame.control.FrameKind());
  EXPECT_EQ(vector.destinationAddressRaw, frame.destination.RawValue());
  EXPECT_EQ(vector.sourceAddressRaw, frame.source.RawValue());
  ASSERT_EQ(vector.informationSize, frame.information.size());

  if (vector.frameKind == HdlcFrameKind::Information) {
    ASSERT_GE(frame.information.size(), 3u);
    EXPECT_EQ(0xe6u, frame.information[0]);
    EXPECT_EQ(0xe6u, frame.information[1]);
    EXPECT_EQ(0x00u, frame.information[2]);
  }

  const std::vector<std::uint8_t> encoded = EncodeDecodedFrame(frame);
  ASSERT_EQ(vector.size, encoded.size()) << vector.name;
  EXPECT_TRUE(std::equal(encoded.begin(), encoded.end(), vector.data))
    << vector.name;
}

INSTANTIATE_TEST_SUITE_P(
  DlmsCosem,
  HdlcRealVectorTest,
  testing::Values(
    HdlcRealVector{
      "SNRM request",
      kSnrmRequest,
      sizeof(kSnrmRequest),
      false,
      HdlcFrameKind::Unnumbered,
      0x90u,
      0x30u,
      0u
    },
    HdlcRealVector{
      "UA response",
      kUaResponse,
      sizeof(kUaResponse),
      false,
      HdlcFrameKind::Unnumbered,
      0x30u,
      0x90u,
      23u
    },
    HdlcRealVector{
      "I-frame with LLC payload",
      kInformationWithLlcPayload,
      sizeof(kInformationWithLlcPayload),
      false,
      HdlcFrameKind::Information,
      0x90u,
      0x30u,
      71u
    },
    HdlcRealVector{
      "RR response",
      kReceiveReady,
      sizeof(kReceiveReady),
      false,
      HdlcFrameKind::Supervisory,
      0x90u,
      0x30u,
      0u
    },
    HdlcRealVector{
      "DISC request",
      kDiscRequest,
      sizeof(kDiscRequest),
      false,
      HdlcFrameKind::Unnumbered,
      0x90u,
      0x30u,
      0u
    },
    HdlcRealVector{
      "Historical SNRM request",
      kHistoricalSnrmRequest,
      sizeof(kHistoricalSnrmRequest),
      false,
      HdlcFrameKind::Unnumbered,
      0x91u,
      0x64u,
      0u
    },
    HdlcRealVector{
      "Historical UA response",
      kHistoricalUaResponse,
      sizeof(kHistoricalUaResponse),
      false,
      HdlcFrameKind::Unnumbered,
      0x64u,
      0x91u,
      21u
    },
    HdlcRealVector{
      "Historical DISC request",
      kHistoricalDiscRequest,
      sizeof(kHistoricalDiscRequest),
      false,
      HdlcFrameKind::Unnumbered,
      0x91u,
      0x64u,
      0u
    },
    HdlcRealVector{
      "Historical RR response",
      kHistoricalReceiveReady,
      sizeof(kHistoricalReceiveReady),
      false,
      HdlcFrameKind::Supervisory,
      0x91u,
      0x64u,
      0u
    },
    HdlcRealVector{
      "Historical I-frame with LLC payload",
      kHistoricalInformationWithLlcPayload,
      sizeof(kHistoricalInformationWithLlcPayload),
      false,
      HdlcFrameKind::Information,
      0x91u,
      0x64u,
      16u
    }));

} // namespace
