#include "dlms/hdlc/hdlc_segmentation.hpp"

#include "dlms/hdlc/hdlc_address.hpp"
#include "dlms/hdlc/hdlc_control.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::DefaultHdlcCodecLimits;
using dlms::hdlc::DlmsHdlcAddress;
using dlms::hdlc::HdlcCodecLimits;
using dlms::hdlc::HdlcControl;
using dlms::hdlc::HdlcFrame;
using dlms::hdlc::HdlcFrameBuffer;
using dlms::hdlc::HdlcReassembler;
using dlms::hdlc::HdlcSegmentationOptions;
using dlms::hdlc::HdlcSegmenter;
using dlms::hdlc::HdlcStatus;

HdlcFrame MakeBaseFrame()
{
  HdlcFrame frame;
  frame.segmented = false;
  EXPECT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeServerAddress(0x01u, 0x11u, frame.destination));
  EXPECT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeClientAddress(0x64u, frame.source));
  EXPECT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x32u, frame.control));
  frame.informationData = 0;
  frame.informationSize = 0u;
  return frame;
}

HdlcSegmentationOptions Options(std::size_t maximumInformationFieldSize)
{
  HdlcSegmentationOptions options;
  options.limits = DefaultHdlcCodecLimits();
  options.limits.maximumInformationFieldSize = maximumInformationFieldSize;
  return options;
}

HdlcFrameBuffer FrameBufferFrom(
  const HdlcFrame& baseFrame,
  bool segmented,
  const std::vector<std::uint8_t>& information)
{
  HdlcFrameBuffer frame;
  frame.segmented = segmented;
  frame.destination = baseFrame.destination;
  frame.source = baseFrame.source;
  frame.control = baseFrame.control;
  frame.information = information;
  return frame;
}

TEST(HdlcSegmenter, SegmentInformationSingleFrame)
{
  const HdlcFrame baseFrame = MakeBaseFrame();
  const std::uint8_t information[] = {0x01, 0x02, 0x03};
  HdlcSegmenter segmenter(Options(4u));
  std::vector<HdlcFrameBuffer> frames;

  ASSERT_EQ(HdlcStatus::Ok,
            segmenter.SegmentInformation(baseFrame,
                                         information,
                                         sizeof(information),
                                         frames));
  ASSERT_EQ(1u, frames.size());
  EXPECT_FALSE(frames[0].segmented);
  EXPECT_EQ(3u, frames[0].information.size());
}

TEST(HdlcSegmenter, SegmentInformationMultipleFrames)
{
  const HdlcFrame baseFrame = MakeBaseFrame();
  const std::uint8_t information[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  HdlcSegmenter segmenter(Options(2u));
  std::vector<HdlcFrameBuffer> frames;

  ASSERT_EQ(HdlcStatus::Ok,
            segmenter.SegmentInformation(baseFrame,
                                         information,
                                         sizeof(information),
                                         frames));
  ASSERT_EQ(3u, frames.size());
  EXPECT_TRUE(frames[0].segmented);
  EXPECT_TRUE(frames[1].segmented);
  EXPECT_FALSE(frames[2].segmented);
  EXPECT_EQ(2u, frames[0].information.size());
  EXPECT_EQ(2u, frames[1].information.size());
  EXPECT_EQ(1u, frames[2].information.size());
}

TEST(HdlcSegmenter, SegmentInformationExactBoundary)
{
  const HdlcFrame baseFrame = MakeBaseFrame();
  const std::uint8_t information[] = {0x01, 0x02, 0x03, 0x04};
  HdlcSegmenter segmenter(Options(2u));
  std::vector<HdlcFrameBuffer> frames;

  ASSERT_EQ(HdlcStatus::Ok,
            segmenter.SegmentInformation(baseFrame,
                                         information,
                                         sizeof(information),
                                         frames));
  ASSERT_EQ(2u, frames.size());
  EXPECT_TRUE(frames[0].segmented);
  EXPECT_FALSE(frames[1].segmented);
}

TEST(HdlcSegmenter, RejectsNullInformationWithNonZeroSize)
{
  const HdlcFrame baseFrame = MakeBaseFrame();
  HdlcSegmenter segmenter(Options(2u));
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::InvalidArgument,
            segmenter.SegmentInformation(baseFrame, 0, 1u, frames));
}

TEST(HdlcReassembler, ReassembleSingleFrame)
{
  const HdlcFrame baseFrame = MakeBaseFrame();
  HdlcReassembler reassembler(DefaultHdlcCodecLimits());
  HdlcFrameBuffer completed;
  bool hasCompleted = false;

  ASSERT_EQ(HdlcStatus::Ok,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  false,
                                                  std::vector<std::uint8_t>{1, 2}),
                                  completed,
                                  hasCompleted));
  EXPECT_TRUE(hasCompleted);
  EXPECT_EQ(2u, completed.information.size());
}

TEST(HdlcReassembler, ReassembleMultipleFrames)
{
  const HdlcFrame baseFrame = MakeBaseFrame();
  HdlcReassembler reassembler(DefaultHdlcCodecLimits());
  HdlcFrameBuffer completed;
  bool hasCompleted = false;

  EXPECT_EQ(HdlcStatus::SegmentationIncomplete,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  true,
                                                  std::vector<std::uint8_t>{1, 2}),
                                  completed,
                                  hasCompleted));
  EXPECT_FALSE(hasCompleted);

  ASSERT_EQ(HdlcStatus::Ok,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  false,
                                                  std::vector<std::uint8_t>{3}),
                                  completed,
                                  hasCompleted));
  ASSERT_TRUE(hasCompleted);
  ASSERT_EQ(3u, completed.information.size());
  EXPECT_EQ(1u, completed.information[0]);
  EXPECT_EQ(2u, completed.information[1]);
  EXPECT_EQ(3u, completed.information[2]);
  EXPECT_FALSE(completed.segmented);
}

TEST(HdlcReassembler, ReassembleAddressMismatch)
{
  HdlcFrame baseFrame = MakeBaseFrame();
  HdlcReassembler reassembler(DefaultHdlcCodecLimits());
  HdlcFrameBuffer completed;
  bool hasCompleted = false;

  ASSERT_EQ(HdlcStatus::SegmentationIncomplete,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  true,
                                                  std::vector<std::uint8_t>{1}),
                                  completed,
                                  hasCompleted));
  EXPECT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeServerAddress(0x01u, 0x12u, baseFrame.destination));
  EXPECT_EQ(HdlcStatus::SegmentationError,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  false,
                                                  std::vector<std::uint8_t>{2}),
                                  completed,
                                  hasCompleted));
}

TEST(HdlcReassembler, ReassembleControlMismatch)
{
  HdlcFrame baseFrame = MakeBaseFrame();
  HdlcReassembler reassembler(DefaultHdlcCodecLimits());
  HdlcFrameBuffer completed;
  bool hasCompleted = false;

  ASSERT_EQ(HdlcStatus::SegmentationIncomplete,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  true,
                                                  std::vector<std::uint8_t>{1}),
                                  completed,
                                  hasCompleted));
  EXPECT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x13u, baseFrame.control));
  EXPECT_EQ(HdlcStatus::SegmentationError,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  false,
                                                  std::vector<std::uint8_t>{2}),
                                  completed,
                                  hasCompleted));
}

TEST(HdlcReassembler, ReassembleOverflow)
{
  const HdlcFrame baseFrame = MakeBaseFrame();
  HdlcCodecLimits limits = DefaultHdlcCodecLimits();
  limits.maximumReassembledInformationSize = 2u;
  HdlcReassembler reassembler(limits);
  HdlcFrameBuffer completed;
  bool hasCompleted = false;

  ASSERT_EQ(HdlcStatus::SegmentationIncomplete,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  true,
                                                  std::vector<std::uint8_t>{1, 2}),
                                  completed,
                                  hasCompleted));
  EXPECT_EQ(HdlcStatus::SegmentationOverflow,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  false,
                                                  std::vector<std::uint8_t>{3}),
                                  completed,
                                  hasCompleted));
}

TEST(HdlcReassembler, ReassembleNewSequenceBeforeCompletion)
{
  HdlcFrame baseFrame = MakeBaseFrame();
  HdlcReassembler reassembler(DefaultHdlcCodecLimits());
  HdlcFrameBuffer completed;
  bool hasCompleted = false;

  ASSERT_EQ(HdlcStatus::SegmentationIncomplete,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  true,
                                                  std::vector<std::uint8_t>{1}),
                                  completed,
                                  hasCompleted));
  EXPECT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeServerAddress(0x01u, 0x12u, baseFrame.destination));
  EXPECT_EQ(HdlcStatus::SegmentationError,
            reassembler.PushFrame(FrameBufferFrom(baseFrame,
                                                  true,
                                                  std::vector<std::uint8_t>{9}),
                                  completed,
                                  hasCompleted));
}

} // namespace
