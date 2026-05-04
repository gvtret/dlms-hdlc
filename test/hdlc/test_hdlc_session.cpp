#include "dlms/hdlc/hdlc_session.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::DecodeFrame;
using dlms::hdlc::DefaultHdlcCodecLimits;
using dlms::hdlc::DlmsHdlcAddress;
using dlms::hdlc::HdlcFrameBuffer;
using dlms::hdlc::HdlcFrameKind;
using dlms::hdlc::HdlcSession;
using dlms::hdlc::HdlcSessionOptions;
using dlms::hdlc::HdlcSessionRole;
using dlms::hdlc::HdlcSessionState;
using dlms::hdlc::HdlcStatus;

HdlcSessionOptions MakeOptions(HdlcSessionRole role)
{
  HdlcSessionOptions options;
  options.role = role;
  EXPECT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeClientAddress(0x64u,
                                               options.clientAddress));
  EXPECT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeServerAddress(0x01u,
                                               0x11u,
                                               options.serverAddress));
  options.limits = DefaultHdlcCodecLimits();
  return options;
}

HdlcFrameBuffer DecodeOrEmpty(const std::vector<std::uint8_t>& bytes)
{
  HdlcFrameBuffer frame;
  EXPECT_EQ(HdlcStatus::Ok,
            DecodeFrame(bytes.empty() ? 0 : &bytes[0],
                        bytes.size(),
                        DefaultHdlcCodecLimits(),
                        frame));
  return frame;
}

void Establish(HdlcSession& client, HdlcSession& server)
{
  std::vector<std::uint8_t> bytes;

  ASSERT_EQ(HdlcStatus::Ok, client.BuildConnectRequest(bytes));
  ASSERT_EQ(HdlcStatus::Ok, server.ReceiveFrame(DecodeOrEmpty(bytes)));

  ASSERT_EQ(HdlcStatus::Ok, server.BuildConnectResponse(bytes));
  ASSERT_EQ(HdlcStatus::Ok, client.ReceiveFrame(DecodeOrEmpty(bytes)));

  EXPECT_EQ(HdlcSessionState::Connected, client.State());
  EXPECT_EQ(HdlcSessionState::Connected, server.State());
}

TEST(HdlcSession, ClientBuildsSnrmConnectRequest)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  std::vector<std::uint8_t> bytes;

  ASSERT_EQ(HdlcStatus::Ok, client.BuildConnectRequest(bytes));
  const HdlcFrameBuffer frame = DecodeOrEmpty(bytes);

  EXPECT_EQ(HdlcSessionState::AwaitingConnection, client.State());
  EXPECT_EQ(HdlcFrameKind::Unnumbered, frame.control.FrameKind());
  EXPECT_TRUE(frame.control.PollFinal());
  EXPECT_EQ(0x91u, frame.destination.RawValue());
  EXPECT_EQ(0x64u, frame.source.RawValue());
}

TEST(HdlcSession, ServerAcceptsSnrmAndBuildsUa)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));
  std::vector<std::uint8_t> bytes;

  ASSERT_EQ(HdlcStatus::Ok, client.BuildConnectRequest(bytes));
  EXPECT_EQ(HdlcStatus::Ok, server.ReceiveFrame(DecodeOrEmpty(bytes)));
  ASSERT_EQ(HdlcStatus::Ok, server.BuildConnectResponse(bytes));

  const HdlcFrameBuffer frame = DecodeOrEmpty(bytes);
  EXPECT_EQ(HdlcSessionState::Connected, server.State());
  EXPECT_EQ(HdlcFrameKind::Unnumbered, frame.control.FrameKind());
  EXPECT_TRUE(frame.control.PollFinal());
  EXPECT_EQ(0x64u, frame.destination.RawValue());
  EXPECT_EQ(0x91u, frame.source.RawValue());
}

TEST(HdlcSession, ClientConnectsAfterUa)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));

  Establish(client, server);

  EXPECT_EQ(0u, client.SendSequence());
  EXPECT_EQ(0u, client.ReceiveSequence());
}

TEST(HdlcSession, InformationFramesAdvanceSequences)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));
  Establish(client, server);

  const std::uint8_t information[] = {0xe6u, 0xe6u, 0x00u};
  std::vector<std::uint8_t> bytes;

  ASSERT_EQ(HdlcStatus::Ok,
            client.BuildInformationFrame(information,
                                         sizeof(information),
                                         true,
                                         bytes));
  EXPECT_EQ(1u, client.SendSequence());

  const HdlcFrameBuffer frame = DecodeOrEmpty(bytes);
  EXPECT_EQ(HdlcFrameKind::Information, frame.control.FrameKind());
  EXPECT_EQ(0u, frame.control.SendSequence());
  EXPECT_EQ(0u, frame.control.ReceiveSequence());

  EXPECT_EQ(HdlcStatus::Ok, server.ReceiveFrame(frame));
  EXPECT_EQ(1u, server.ReceiveSequence());
}

TEST(HdlcSession, ReceiveReadyAcknowledgesSentInformation)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));
  Establish(client, server);

  const std::uint8_t information[] = {0xe6u, 0xe6u, 0x00u};
  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok,
            client.BuildInformationFrame(information,
                                         sizeof(information),
                                         false,
                                         bytes));
  ASSERT_EQ(HdlcStatus::Ok, server.ReceiveFrame(DecodeOrEmpty(bytes)));

  ASSERT_EQ(HdlcStatus::Ok, server.BuildReceiveReadyFrame(true, bytes));
  const HdlcFrameBuffer rr = DecodeOrEmpty(bytes);

  EXPECT_EQ(HdlcFrameKind::Supervisory, rr.control.FrameKind());
  EXPECT_EQ(1u, rr.control.ReceiveSequence());
  EXPECT_EQ(HdlcStatus::Ok, client.ReceiveFrame(rr));
}

TEST(HdlcSession, RejectsUnexpectedInformationSequence)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));
  Establish(client, server);

  const std::uint8_t information[] = {0xe6u};
  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok,
            client.BuildInformationFrame(information,
                                         sizeof(information),
                                         false,
                                         bytes));

  HdlcFrameBuffer frame = DecodeOrEmpty(bytes);
  EXPECT_EQ(HdlcStatus::Ok, server.ReceiveFrame(frame));
  EXPECT_EQ(HdlcStatus::InvalidControlField, server.ReceiveFrame(frame));
}

TEST(HdlcSession, RejectsWrongIncomingAddress)
{
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  std::vector<std::uint8_t> bytes;

  ASSERT_EQ(HdlcStatus::Ok, client.BuildConnectRequest(bytes));
  HdlcFrameBuffer frame = DecodeOrEmpty(bytes);
  ASSERT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeClientAddress(0x10u, frame.destination));

  EXPECT_EQ(HdlcStatus::InvalidAddress, server.ReceiveFrame(frame));
}

TEST(HdlcSession, DisconnectExchangeReturnsBothEndpointsToDisconnected)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));
  Establish(client, server);

  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok, client.BuildDisconnectRequest(bytes));
  EXPECT_EQ(HdlcSessionState::AwaitingDisconnect, client.State());
  ASSERT_EQ(HdlcStatus::Ok, server.ReceiveFrame(DecodeOrEmpty(bytes)));

  ASSERT_EQ(HdlcStatus::Ok, server.BuildDisconnectResponse(bytes));
  EXPECT_EQ(HdlcSessionState::Disconnected, server.State());
  EXPECT_EQ(HdlcStatus::Ok, client.ReceiveFrame(DecodeOrEmpty(bytes)));
  EXPECT_EQ(HdlcSessionState::Disconnected, client.State());
}

} // namespace
