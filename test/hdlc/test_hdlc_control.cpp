#include "dlms/hdlc/hdlc_control.hpp"

#include <cstdint>

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::HdlcControl;
using dlms::hdlc::HdlcFrameKind;
using dlms::hdlc::HdlcSupervisoryKind;
using dlms::hdlc::HdlcStatus;
using dlms::hdlc::HdlcUnnumberedKind;

void ExpectRoundtrip(std::uint8_t value)
{
  HdlcControl control;
  std::uint8_t encoded = 0u;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(value, control));
  EXPECT_EQ(HdlcStatus::Ok, control.Encode(encoded));
  EXPECT_EQ(value, encoded);
}

TEST(HdlcControl, DecodeIFrame)
{
  HdlcControl control;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x30u, control));
  EXPECT_EQ(HdlcFrameKind::Information, control.FrameKind());
  EXPECT_TRUE(control.PollFinal());
  EXPECT_EQ(0u, control.SendSequence());
  EXPECT_EQ(1u, control.ReceiveSequence());
}

TEST(HdlcControl, DecodeIFrameWithoutPollFinal)
{
  HdlcControl control;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x42u, control));
  EXPECT_EQ(HdlcFrameKind::Information, control.FrameKind());
  EXPECT_FALSE(control.PollFinal());
  EXPECT_EQ(1u, control.SendSequence());
  EXPECT_EQ(2u, control.ReceiveSequence());
}

TEST(HdlcControl, DecodeReceiveReady)
{
  HdlcControl control;
  HdlcSupervisoryKind kind = HdlcSupervisoryKind::Reject;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x11u, control));
  EXPECT_EQ(HdlcFrameKind::Supervisory, control.FrameKind());
  EXPECT_EQ(HdlcStatus::Ok, control.SupervisoryKind(kind));
  EXPECT_EQ(HdlcSupervisoryKind::ReceiveReady, kind);
  EXPECT_TRUE(control.PollFinal());
  EXPECT_EQ(0u, control.SendSequence());
  EXPECT_EQ(0u, control.ReceiveSequence());
}

TEST(HdlcControl, DecodeReceiveNotReady)
{
  HdlcControl control;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x25u, control));
  EXPECT_EQ(HdlcFrameKind::Supervisory, control.FrameKind());
  EXPECT_FALSE(control.PollFinal());
  EXPECT_EQ(1u, control.ReceiveSequence());
}

TEST(HdlcControl, DecodeReject)
{
  HdlcControl control;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x49u, control));
  EXPECT_EQ(HdlcFrameKind::Supervisory, control.FrameKind());
  EXPECT_EQ(2u, control.ReceiveSequence());
}

TEST(HdlcControl, DecodeSelectiveReject)
{
  HdlcControl control;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x6du, control));
  EXPECT_EQ(HdlcFrameKind::Supervisory, control.FrameKind());
  EXPECT_EQ(3u, control.ReceiveSequence());
}

TEST(HdlcControl, DecodeSnrm)
{
  HdlcControl control;
  HdlcUnnumberedKind kind = HdlcUnnumberedKind::Ua;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x93u, control));
  EXPECT_EQ(HdlcFrameKind::Unnumbered, control.FrameKind());
  EXPECT_EQ(HdlcStatus::Ok, control.UnnumberedKind(kind));
  EXPECT_EQ(HdlcUnnumberedKind::Snrm, kind);
  EXPECT_TRUE(control.PollFinal());
}

TEST(HdlcControl, DecodeUa)
{
  HdlcControl control;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x73u, control));
  EXPECT_EQ(HdlcFrameKind::Unnumbered, control.FrameKind());
  EXPECT_TRUE(control.PollFinal());
}

TEST(HdlcControl, DecodeDisc)
{
  HdlcControl control;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x53u, control));
  EXPECT_EQ(HdlcFrameKind::Unnumbered, control.FrameKind());
  EXPECT_TRUE(control.PollFinal());
}

TEST(HdlcControl, DecodeDm)
{
  HdlcControl control;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x1fu, control));
  EXPECT_EQ(HdlcFrameKind::Unnumbered, control.FrameKind());
  EXPECT_TRUE(control.PollFinal());
}

TEST(HdlcControl, DecodeFrmr)
{
  HdlcControl control;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x97u, control));
  EXPECT_EQ(HdlcFrameKind::Unnumbered, control.FrameKind());
  EXPECT_TRUE(control.PollFinal());
}

TEST(HdlcControl, DecodeUi)
{
  HdlcControl control;

  ASSERT_EQ(HdlcStatus::Ok, HdlcControl::Decode(0x13u, control));
  EXPECT_EQ(HdlcFrameKind::Unnumbered, control.FrameKind());
  EXPECT_TRUE(control.PollFinal());
}

TEST(HdlcControl, DecodeRejectsUnsupportedUnnumbered)
{
  HdlcControl control;

  EXPECT_EQ(HdlcStatus::InvalidControlField,
            HdlcControl::Decode(0x33u, control));
}

TEST(HdlcControl, EncodeRoundtrip)
{
  ExpectRoundtrip(0x30u);
  ExpectRoundtrip(0x11u);
  ExpectRoundtrip(0x25u);
  ExpectRoundtrip(0x49u);
  ExpectRoundtrip(0x6du);
  ExpectRoundtrip(0x93u);
  ExpectRoundtrip(0x73u);
  ExpectRoundtrip(0x53u);
  ExpectRoundtrip(0x1fu);
  ExpectRoundtrip(0x97u);
  ExpectRoundtrip(0x13u);
}

} // namespace
