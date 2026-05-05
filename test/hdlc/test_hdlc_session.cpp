#include "dlms/hdlc/hdlc_session.hpp"
#include "dlms/hdlc/hdlc_codec.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::DecodeFrame;
using dlms::hdlc::DefaultHdlcCodecLimits;
using dlms::hdlc::DlmsHdlcAddress;
using dlms::hdlc::EncodeFrame;
using dlms::hdlc::HdlcFrame;
using dlms::hdlc::HdlcFrameBuffer;
using dlms::hdlc::HdlcFrameKind;
using dlms::hdlc::HdlcSession;
using dlms::hdlc::HdlcSessionOptions;
using dlms::hdlc::HdlcSessionRole;
using dlms::hdlc::HdlcSessionState;
using dlms::hdlc::HdlcStatus;
using dlms::hdlc::HdlcUnnumberedKind;

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

// --------------------------------------------------------------------------
// Phase 13: SNRM/UA parameter negotiation
// --------------------------------------------------------------------------

HdlcSessionOptions MakeOptionsWithLimits(
  HdlcSessionRole role,
  std::size_t maxInfoTx,
  std::size_t maxInfoRx,
  std::uint8_t windowTx,
  std::uint8_t windowRx)
{
  HdlcSessionOptions opts = MakeOptions(role);
  opts.negotiationLimits.maxInformationFieldLengthTransmit = maxInfoTx;
  opts.negotiationLimits.maxInformationFieldLengthReceive  = maxInfoRx;
  opts.negotiationLimits.windowSizeTransmit = windowTx;
  opts.negotiationLimits.windowSizeReceive  = windowRx;
  return opts;
}

TEST(HdlcSession, SnrmWithDefaultParameters_ClientSendsNoInfoField)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  std::vector<std::uint8_t> bytes;

  ASSERT_EQ(HdlcStatus::Ok, client.BuildConnectRequest(bytes));

  HdlcFrameBuffer frame;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(bytes.data(), bytes.size(), DefaultHdlcCodecLimits(), frame));
  EXPECT_TRUE(frame.information.empty());
}

TEST(HdlcSession, SnrmWithMaxInfoFieldLength_ServerNegotiates)
{
  // Client proposes max_info_tx=512; server can only receive 256.
  HdlcSession client(MakeOptionsWithLimits(
    HdlcSessionRole::Client, 512u, 512u, 1u, 1u));
  HdlcSession server(MakeOptionsWithLimits(
    HdlcSessionRole::Server, 256u, 256u, 1u, 1u));

  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok, client.BuildConnectRequest(bytes));

  HdlcFrameBuffer snrm;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(bytes.data(), bytes.size(), DefaultHdlcCodecLimits(), snrm));
  EXPECT_FALSE(snrm.information.empty());

  ASSERT_EQ(HdlcStatus::Ok, server.ReceiveFrame(snrm));
  ASSERT_EQ(HdlcStatus::Ok, server.BuildConnectResponse(bytes));

  // Negotiated transmit limit on server = min(512, 256) = 256.
  EXPECT_EQ(256u, server.NegotiatedLimits().maxInformationFieldLengthTransmit);
}

TEST(HdlcSession, SnrmWithWindowSize_ServerNegotiates)
{
  // Client proposes window=4; server supports window=3.
  HdlcSession client(MakeOptionsWithLimits(
    HdlcSessionRole::Client, 128u, 128u, 4u, 4u));
  HdlcSession server(MakeOptionsWithLimits(
    HdlcSessionRole::Server, 128u, 128u, 3u, 3u));

  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok, client.BuildConnectRequest(bytes));

  HdlcFrameBuffer snrm;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(bytes.data(), bytes.size(), DefaultHdlcCodecLimits(), snrm));

  ASSERT_EQ(HdlcStatus::Ok, server.ReceiveFrame(snrm));
  ASSERT_EQ(HdlcStatus::Ok, server.BuildConnectResponse(bytes));

  // Server transmit window = min(window_rx_from_client=4, server_windowTx=3) = 3.
  EXPECT_EQ(3u, server.NegotiatedLimits().windowSizeTransmit);
}

TEST(HdlcSession, UaCarriesNegotiatedValues)
{
  HdlcSession client(MakeOptionsWithLimits(
    HdlcSessionRole::Client, 256u, 256u, 2u, 2u));
  HdlcSession server(MakeOptionsWithLimits(
    HdlcSessionRole::Server, 128u, 128u, 1u, 1u));

  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok, client.BuildConnectRequest(bytes));

  HdlcFrameBuffer snrm;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(bytes.data(), bytes.size(), DefaultHdlcCodecLimits(), snrm));
  ASSERT_EQ(HdlcStatus::Ok, server.ReceiveFrame(snrm));
  ASSERT_EQ(HdlcStatus::Ok, server.BuildConnectResponse(bytes));

  // UA must have a non-empty info field containing negotiated params.
  HdlcFrameBuffer ua;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(bytes.data(), bytes.size(), DefaultHdlcCodecLimits(), ua));
  EXPECT_FALSE(ua.information.empty());
  // First byte of info = 0x81 (format identifier).
  ASSERT_GE(ua.information.size(), 1u);
  EXPECT_EQ(0x81u, ua.information[0]);
}

TEST(HdlcSession, CodecLimitsUpdatedAfterNegotiation)
{
  HdlcSession client(MakeOptionsWithLimits(
    HdlcSessionRole::Client, 512u, 512u, 1u, 1u));
  HdlcSession server(MakeOptionsWithLimits(
    HdlcSessionRole::Server, 200u, 200u, 1u, 1u));

  std::vector<std::uint8_t> snrmBytes;
  ASSERT_EQ(HdlcStatus::Ok, client.BuildConnectRequest(snrmBytes));

  HdlcFrameBuffer snrm;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(snrmBytes.data(), snrmBytes.size(),
                        DefaultHdlcCodecLimits(), snrm));
  ASSERT_EQ(HdlcStatus::Ok, server.ReceiveFrame(snrm));

  std::vector<std::uint8_t> uaBytes;
  ASSERT_EQ(HdlcStatus::Ok, server.BuildConnectResponse(uaBytes));

  HdlcFrameBuffer ua;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(uaBytes.data(), uaBytes.size(),
                        DefaultHdlcCodecLimits(), ua));
  ASSERT_EQ(HdlcStatus::Ok, client.ReceiveFrame(ua));

  // After negotiation both sides should have converged on min(512, 200) = 200.
  EXPECT_EQ(200u,
            client.NegotiatedLimits().maxInformationFieldLengthTransmit);
  EXPECT_EQ(200u,
            server.NegotiatedLimits().maxInformationFieldLengthTransmit);
}

TEST(HdlcSession, SnrmWithUnrecognisedParameter_ServerRejectsDm)
{
  // Craft a SNRM info field with an unknown tag (0x09).
  // Format: 81 80 04 09 02 01 00
  const std::uint8_t kBadSnrmInfo[] = {
    0x81u, 0x80u, 0x04u,  // header, group len = 4
    0x09u, 0x02u, 0x01u, 0x00u  // unknown tag 09h
  };

  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));

  // Build a SNRM with the bad info field by hand.
  std::vector<std::uint8_t> snrmBytes;
  ASSERT_EQ(HdlcStatus::Ok,
            client.BuildConnectRequest(
              kBadSnrmInfo, sizeof(kBadSnrmInfo), snrmBytes));

  // Inject the bad info field: rebuild the raw frame substituting the info.
  // Simplest: decode the clean SNRM, inject bad info, re-encode.
  HdlcFrameBuffer snrm;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(snrmBytes.data(), snrmBytes.size(),
                        DefaultHdlcCodecLimits(), snrm));
  // Replace info with bad bytes (includes the negotiation block with bad tag).
  snrm.information.assign(kBadSnrmInfo, kBadSnrmInfo + sizeof(kBadSnrmInfo));

  // Re-encode so the server can decode it.
  HdlcFrame rawFrame;
  rawFrame.segmented       = snrm.segmented;
  rawFrame.destination     = snrm.destination;
  rawFrame.source          = snrm.source;
  rawFrame.control         = snrm.control;
  rawFrame.informationData = snrm.information.data();
  rawFrame.informationSize = snrm.information.size();

  std::vector<std::uint8_t> badSnrmBytes;
  ASSERT_EQ(HdlcStatus::Ok,
            EncodeFrame(rawFrame, DefaultHdlcCodecLimits(), badSnrmBytes));

  HdlcFrameBuffer badSnrm;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(badSnrmBytes.data(), badSnrmBytes.size(),
                        DefaultHdlcCodecLimits(), badSnrm));

  EXPECT_NE(HdlcStatus::Ok, server.ReceiveFrame(badSnrm));
  EXPECT_EQ(HdlcSessionState::Disconnected, server.State());

  // Server must be able to send a DM rejection.
  std::vector<std::uint8_t> dmBytes;
  ASSERT_EQ(HdlcStatus::Ok, server.BuildDmResponse(dmBytes));

  HdlcFrameBuffer dm;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(dmBytes.data(), dmBytes.size(),
                        DefaultHdlcCodecLimits(), dm));
  EXPECT_EQ(HdlcFrameKind::Unnumbered, dm.control.FrameKind());

  HdlcUnnumberedKind kind;
  ASSERT_EQ(HdlcStatus::Ok, dm.control.UnnumberedKind(kind));
  EXPECT_EQ(HdlcUnnumberedKind::Dm, kind);
}

// --------------------------------------------------------------------------
// Phase 14: Sliding window
// --------------------------------------------------------------------------

TEST(HdlcSession, WindowSize1_CannotSendSecondFrameBeforeRr)
{
  // Default negotiation limits: window = 1.
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));
  Establish(client, server);

  const std::uint8_t data[] = {0x01u};
  std::vector<std::uint8_t> bytes;

  ASSERT_EQ(HdlcStatus::Ok,
            client.BuildInformationFrame(data, sizeof(data), false, bytes));
  EXPECT_EQ(HdlcStatus::UnsupportedFrame,
            client.BuildInformationFrame(data, sizeof(data), false, bytes));
}

TEST(HdlcSession, WindowSize3_CanSendThreeFramesBeforeRr)
{
  HdlcSession client(MakeOptionsWithLimits(
    HdlcSessionRole::Client, 128u, 128u, 3u, 3u));
  HdlcSession server(MakeOptionsWithLimits(
    HdlcSessionRole::Server, 128u, 128u, 3u, 3u));
  Establish(client, server);

  // After establish the window is still 1 (no SNRM negotiation happened since
  // both sides used default BuildConnectRequest).  Re-establish with explicit
  // negotiation.
  HdlcSession c2(MakeOptionsWithLimits(
    HdlcSessionRole::Client, 128u, 128u, 3u, 3u));
  HdlcSession s2(MakeOptionsWithLimits(
    HdlcSessionRole::Server, 128u, 128u, 3u, 3u));

  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok, c2.BuildConnectRequest(bytes));

  HdlcFrameBuffer snrm;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(bytes.data(), bytes.size(), DefaultHdlcCodecLimits(), snrm));
  ASSERT_EQ(HdlcStatus::Ok, s2.ReceiveFrame(snrm));
  ASSERT_EQ(HdlcStatus::Ok, s2.BuildConnectResponse(bytes));

  HdlcFrameBuffer ua;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(bytes.data(), bytes.size(), DefaultHdlcCodecLimits(), ua));
  ASSERT_EQ(HdlcStatus::Ok, c2.ReceiveFrame(ua));

  EXPECT_EQ(HdlcSessionState::Connected, c2.State());
  EXPECT_EQ(3u, c2.NegotiatedLimits().windowSizeTransmit);

  const std::uint8_t data[] = {0x01u};
  EXPECT_EQ(HdlcStatus::Ok,
            c2.BuildInformationFrame(data, sizeof(data), false, bytes));
  EXPECT_EQ(HdlcStatus::Ok,
            c2.BuildInformationFrame(data, sizeof(data), false, bytes));
  EXPECT_EQ(HdlcStatus::Ok,
            c2.BuildInformationFrame(data, sizeof(data), false, bytes));
  // Fourth frame exceeds window=3.
  EXPECT_EQ(HdlcStatus::UnsupportedFrame,
            c2.BuildInformationFrame(data, sizeof(data), false, bytes));
}

TEST(HdlcSession, RrAdvancesAcknowledgeSequence)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));
  Establish(client, server);

  const std::uint8_t data[] = {0x01u};
  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok,
            client.BuildInformationFrame(data, sizeof(data), false, bytes));
  ASSERT_EQ(HdlcStatus::Ok, server.ReceiveFrame(DecodeOrEmpty(bytes)));

  EXPECT_EQ(0u, client.AcknowledgeSequence());

  ASSERT_EQ(HdlcStatus::Ok, server.BuildReceiveReadyFrame(true, bytes));
  ASSERT_EQ(HdlcStatus::Ok, client.ReceiveFrame(DecodeOrEmpty(bytes)));

  EXPECT_EQ(1u, client.AcknowledgeSequence());
  EXPECT_TRUE(client.CanSendInformationFrame());
}

TEST(HdlcSession, WindowExhaustedBlocksSend)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));
  Establish(client, server);

  const std::uint8_t data[] = {0x02u};
  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok,
            client.BuildInformationFrame(data, sizeof(data), false, bytes));
  EXPECT_FALSE(client.CanSendInformationFrame());
  EXPECT_EQ(HdlcStatus::UnsupportedFrame,
            client.BuildInformationFrame(data, sizeof(data), false, bytes));
}

// --------------------------------------------------------------------------
// Phase 15: User_Information passthrough in SNRM and DISC
// --------------------------------------------------------------------------

TEST(HdlcSession, SnrmWithUserInformation_ServerReceivesPayload)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));

  const std::uint8_t userInfo[] = {0xc0u, 0x01u, 0x00u};
  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok,
            client.BuildConnectRequest(userInfo, sizeof(userInfo), bytes));

  HdlcFrameBuffer snrm;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(bytes.data(), bytes.size(), DefaultHdlcCodecLimits(), snrm));
  ASSERT_EQ(HdlcStatus::Ok, server.ReceiveFrame(snrm));

  const std::vector<std::uint8_t>& received = server.ReceivedUserInformation();
  ASSERT_EQ(sizeof(userInfo), received.size());
  EXPECT_EQ(0xc0u, received[0]);
  EXPECT_EQ(0x01u, received[1]);
  EXPECT_EQ(0x00u, received[2]);
}

TEST(HdlcSession, DiscWithUserInformation_PeerReceivesPayload)
{
  HdlcSession client(MakeOptions(HdlcSessionRole::Client));
  HdlcSession server(MakeOptions(HdlcSessionRole::Server));
  Establish(client, server);

  const std::uint8_t userInfo[] = {0xc0u, 0x02u};
  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(HdlcStatus::Ok,
            client.BuildDisconnectRequest(userInfo, sizeof(userInfo), bytes));

  HdlcFrameBuffer disc;
  ASSERT_EQ(HdlcStatus::Ok,
            DecodeFrame(bytes.data(), bytes.size(), DefaultHdlcCodecLimits(), disc));
  ASSERT_EQ(HdlcStatus::Ok, server.ReceiveFrame(disc));

  const std::vector<std::uint8_t>& received = server.ReceivedUserInformation();
  ASSERT_EQ(sizeof(userInfo), received.size());
  EXPECT_EQ(0xc0u, received[0]);
  EXPECT_EQ(0x02u, received[1]);
}

} // namespace
