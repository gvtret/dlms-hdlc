#include "dlms/hdlc/hdlc_error.hpp"

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::HdlcStatus;

TEST(HdlcStatus, ValuesMatchDocumentedOrder)
{
  EXPECT_EQ(0, static_cast<int>(HdlcStatus::Ok));
  EXPECT_EQ(1, static_cast<int>(HdlcStatus::NeedMoreData));
  EXPECT_EQ(2, static_cast<int>(HdlcStatus::OutputBufferTooSmall));
  EXPECT_EQ(3, static_cast<int>(HdlcStatus::InvalidArgument));
  EXPECT_EQ(4, static_cast<int>(HdlcStatus::InvalidFlag));
  EXPECT_EQ(5, static_cast<int>(HdlcStatus::InvalidFrameFormat));
  EXPECT_EQ(6, static_cast<int>(HdlcStatus::InvalidFrameType));
  EXPECT_EQ(7, static_cast<int>(HdlcStatus::InvalidFrameLength));
  EXPECT_EQ(8, static_cast<int>(HdlcStatus::InvalidAddress));
  EXPECT_EQ(9, static_cast<int>(HdlcStatus::InvalidControlField));
  EXPECT_EQ(10, static_cast<int>(HdlcStatus::InvalidHeaderChecksum));
  EXPECT_EQ(11, static_cast<int>(HdlcStatus::InvalidFrameChecksum));
  EXPECT_EQ(12, static_cast<int>(HdlcStatus::FrameTooLarge));
  EXPECT_EQ(13, static_cast<int>(HdlcStatus::InformationFieldTooLarge));
  EXPECT_EQ(14, static_cast<int>(HdlcStatus::SegmentationError));
  EXPECT_EQ(15, static_cast<int>(HdlcStatus::SegmentationIncomplete));
  EXPECT_EQ(16, static_cast<int>(HdlcStatus::SegmentationOverflow));
  EXPECT_EQ(17, static_cast<int>(HdlcStatus::UnsupportedFrame));
  EXPECT_EQ(18, static_cast<int>(HdlcStatus::UnsupportedAddress));
  EXPECT_EQ(19, static_cast<int>(HdlcStatus::UnsupportedFeature));
  EXPECT_EQ(20, static_cast<int>(HdlcStatus::InternalError));
}

} // namespace
