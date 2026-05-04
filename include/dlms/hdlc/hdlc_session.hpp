#pragma once

#include "dlms/hdlc/hdlc_codec.hpp"
#include "dlms/hdlc/hdlc_frame.hpp"
#include "dlms/hdlc/hdlc_types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace hdlc {

/**
 * @brief Endpoint role used by the HDLC session state machine.
 */
enum class HdlcSessionRole
{
  /// Client endpoint that initiates SNRM and DISC.
  Client,
  /// Server endpoint that accepts SNRM and DISC.
  Server
};

/**
 * @brief Current HDLC session state.
 */
enum class HdlcSessionState
{
  /// No active HDLC data-link session.
  Disconnected,
  /// Client has sent SNRM and is waiting for UA.
  AwaitingConnection,
  /// Session is established and I/S frames may be exchanged.
  Connected,
  /// Local endpoint has sent DISC and is waiting for UA.
  AwaitingDisconnect
};

/**
 * @brief Fixed configuration for one HDLC session endpoint.
 */
struct HdlcSessionOptions
{
  /// Local endpoint role.
  HdlcSessionRole role;
  /// Client HDLC address used by this association.
  HdlcAddress clientAddress;
  /// Server HDLC address used by this association.
  HdlcAddress serverAddress;
  /// Codec limits used when building outgoing frames.
  HdlcCodecLimits limits;
};

/**
 * @brief Minimal HDLC session state machine built on top of the frame codec.
 *
 * The session validates endpoint addresses, performs the SNRM/UA and DISC/UA
 * state transitions, and tracks three-bit I-frame send/receive sequence
 * numbers. It does not own transport I/O, timers, retransmission, or APDU/LLC
 * parsing.
 */
class HdlcSession
{
public:
  /**
   * @brief Create a session endpoint.
   *
   * @param options Role, addresses, and codec limits for this endpoint.
   */
  explicit HdlcSession(const HdlcSessionOptions& options);

  /// Return the current session state.
  HdlcSessionState State() const;
  /// Return the next local I-frame send sequence number.
  std::uint8_t SendSequence() const;
  /// Return the next expected remote I-frame send sequence number.
  std::uint8_t ReceiveSequence() const;

  /**
   * @brief Build a client SNRM request and enter AwaitingConnection.
   *
   * @param output Receives the encoded HDLC frame.
   * @return `Ok`, `UnsupportedFeature` for server role, or codec status.
   */
  HdlcStatus BuildConnectRequest(std::vector<std::uint8_t>& output);

  /**
   * @brief Build a server UA response after receiving SNRM.
   *
   * @param output Receives the encoded HDLC frame.
   * @return `Ok`, `UnsupportedFeature` for client role, or state/codec status.
   */
  HdlcStatus BuildConnectResponse(std::vector<std::uint8_t>& output);

  /**
   * @brief Build a DISC request and enter AwaitingDisconnect.
   *
   * @param output Receives the encoded HDLC frame.
   * @return `Ok` or a state/codec status.
   */
  HdlcStatus BuildDisconnectRequest(std::vector<std::uint8_t>& output);

  /**
   * @brief Build a UA response after receiving DISC and enter Disconnected.
   *
   * @param output Receives the encoded HDLC frame.
   * @return `Ok` or a state/codec status.
   */
  HdlcStatus BuildDisconnectResponse(std::vector<std::uint8_t>& output);

  /**
   * @brief Build one I-frame and advance the local send sequence on success.
   *
   * @param information Opaque Information field bytes.
   * @param informationSize Number of Information field bytes.
   * @param pollFinal Poll/Final bit for the outgoing I-frame.
   * @param output Receives the encoded HDLC frame.
   * @return `Ok` or a state/codec status.
   */
  HdlcStatus BuildInformationFrame(
    const std::uint8_t* information,
    std::size_t informationSize,
    bool pollFinal,
    std::vector<std::uint8_t>& output);

  /**
   * @brief Build a Receive Ready supervisory frame.
   *
   * @param pollFinal Poll/Final bit for the outgoing RR frame.
   * @param output Receives the encoded HDLC frame.
   * @return `Ok` or a state/codec status.
   */
  HdlcStatus BuildReceiveReadyFrame(
    bool pollFinal,
    std::vector<std::uint8_t>& output);

  /**
   * @brief Consume an incoming decoded frame and update session state.
   *
   * @param frame Decoded frame from the existing HDLC codec.
   * @return `Ok` when accepted, otherwise address/control/state status.
   */
  HdlcStatus ReceiveFrame(const HdlcFrameBuffer& frame);

private:
  HdlcStatus BuildUnnumberedFrame(
    std::uint8_t control,
    std::vector<std::uint8_t>& output) const;
  HdlcStatus BuildSupervisoryFrame(
    std::uint8_t control,
    std::vector<std::uint8_t>& output) const;
  HdlcStatus BuildFrame(
    std::uint8_t control,
    const std::uint8_t* information,
    std::size_t informationSize,
    std::vector<std::uint8_t>& output) const;

  HdlcStatus ReceiveInformationFrame(const HdlcFrameBuffer& frame);
  HdlcStatus ReceiveSupervisoryFrame(const HdlcFrameBuffer& frame);
  HdlcStatus ReceiveUnnumberedFrame(const HdlcFrameBuffer& frame);

  bool IncomingAddressesMatch(const HdlcFrameBuffer& frame) const;
  const HdlcAddress& LocalAddress() const;
  const HdlcAddress& RemoteAddress() const;

  HdlcSessionOptions options_;
  HdlcSessionState state_;
  std::uint8_t sendSequence_;
  std::uint8_t receiveSequence_;
  bool connectResponsePending_;
  bool disconnectResponsePending_;
};

} // namespace hdlc
} // namespace dlms
