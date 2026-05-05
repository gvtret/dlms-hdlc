#pragma once

#include "dlms/hdlc/hdlc_address.hpp"
#include "dlms/hdlc/hdlc_error.hpp"
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
 * @brief Local endpoint negotiation capabilities for SNRM/UA parameter exchange.
 *
 * Zero fields are replaced with protocol defaults (max_info=128, window=1) by
 * the HdlcSession constructor, so callers need not fill every field explicitly.
 */
struct HdlcSessionNegotiationLimits
{
  /// Maximum Information field bytes this endpoint will transmit per frame.
  std::size_t maxInformationFieldLengthTransmit = 0;
  /// Maximum Information field bytes this endpoint can receive per frame.
  std::size_t maxInformationFieldLengthReceive = 0;
  /// Transmit window size (1-7) this endpoint will use.
  std::uint8_t windowSizeTransmit = 0;
  /// Receive window size (1-7) this endpoint can handle.
  std::uint8_t windowSizeReceive = 0;
};

/**
 * @brief Return the HDLC default negotiation limits (max_info=128, window=1).
 */
HdlcSessionNegotiationLimits DefaultHdlcSessionNegotiationLimits();

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
  /// Local negotiation capabilities. Zero fields default to protocol values.
  HdlcSessionNegotiationLimits negotiationLimits;
};

/**
 * @brief Minimal HDLC session state machine built on top of the frame codec.
 *
 * The session validates endpoint addresses, performs the SNRM/UA and DISC/UA
 * state transitions, tracks three-bit I-frame send/receive sequence numbers,
 * supports SNRM/UA parameter negotiation, and enforces the negotiated sliding
 * window size. It does not own transport I/O, timers, retransmission, or
 * APDU/LLC parsing.
 */
class HdlcSession
{
public:
  /**
   * @brief Create a session endpoint.
   *
   * Zero fields in @p options.negotiationLimits are replaced with protocol
   * defaults. The initial negotiated limits equal the normalised local limits.
   *
   * @param options Role, addresses, codec limits, and negotiation capabilities.
   */
  explicit HdlcSession(const HdlcSessionOptions& options);

  /// Return the current session state.
  HdlcSessionState State() const;
  /// Return the next local I-frame send sequence number V(S).
  std::uint8_t SendSequence() const;
  /// Return the next expected remote I-frame send sequence number V(R).
  std::uint8_t ReceiveSequence() const;
  /// Return the last acknowledged send sequence variable V(A).
  std::uint8_t AcknowledgeSequence() const;

  /**
   * @brief Return true when an I-frame can be sent without exceeding the window.
   *
   * An I-frame can be sent when outstanding = V(S) - V(A) mod 8 is less than
   * the negotiated transmit window size.
   */
  bool CanSendInformationFrame() const;

  /**
   * @brief Return the negotiated session limits after a successful SNRM/UA exchange.
   *
   * Before the first successful connection the returned values reflect the
   * normalised local capabilities from @p options.negotiationLimits.
   */
  const HdlcSessionNegotiationLimits& NegotiatedLimits() const;

  /**
   * @brief Return the User_Information bytes extracted from the last received
   *        SNRM or DISC frame.
   *
   * The buffer is empty when the last frame carried no user_information.
   */
  const std::vector<std::uint8_t>& ReceivedUserInformation() const;

  /**
   * @brief Build a client SNRM request and enter AwaitingConnection.
   *
   * An HDLC Information field with negotiation parameters is included when the
   * local negotiation limits differ from the protocol defaults.
   *
   * @param output Receives the encoded HDLC frame.
   * @return `Ok`, `UnsupportedFeature` for server role, or codec status.
   */
  HdlcStatus BuildConnectRequest(std::vector<std::uint8_t>& output);

  /**
   * @brief Build a client SNRM request with an optional User_Information field.
   *
   * The HDLC Information field always includes negotiation parameters followed
   * by the supplied @p userInformation bytes.
   *
   * @param userInformation Optional opaque bytes from the application layer.
   * @param userInformationSize Number of @p userInformation bytes (may be 0).
   * @param output Receives the encoded HDLC frame.
   * @return `Ok`, `UnsupportedFeature` for server role, or codec status.
   */
  HdlcStatus BuildConnectRequest(
    const std::uint8_t* userInformation,
    std::size_t userInformationSize,
    std::vector<std::uint8_t>& output);

  /**
   * @brief Build a server UA response after receiving SNRM.
   *
   * When the SNRM carried negotiation parameters the UA includes the negotiated
   * values and codec limits are updated accordingly.
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
   * @brief Build a DISC request carrying optional User_Information.
   *
   * @param userInformation Optional opaque bytes from the application layer.
   * @param userInformationSize Number of @p userInformation bytes (may be 0).
   * @param output Receives the encoded HDLC frame.
   * @return `Ok` or a state/codec status.
   */
  HdlcStatus BuildDisconnectRequest(
    const std::uint8_t* userInformation,
    std::size_t userInformationSize,
    std::vector<std::uint8_t>& output);

  /**
   * @brief Build a UA response after receiving DISC and enter Disconnected.
   *
   * @param output Receives the encoded HDLC frame.
   * @return `Ok` or a state/codec status.
   */
  HdlcStatus BuildDisconnectResponse(std::vector<std::uint8_t>& output);

  /**
   * @brief Build a DM response to reject a received SNRM with unrecognised parameters.
   *
   * Only valid when a previous ReceiveFrame call returned a non-Ok status for
   * a SNRM with unrecognised parameter identifiers.
   *
   * @param output Receives the encoded HDLC frame.
   * @return `Ok`, `UnsupportedFrame` if no DM response is pending, or codec status.
   */
  HdlcStatus BuildDmResponse(std::vector<std::uint8_t>& output);

  /**
   * @brief Build one I-frame and advance the local send sequence on success.
   *
   * Returns `UnsupportedFrame` when the sliding window is exhausted.
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
  HdlcStatus BuildConnectRequestInternal(
    bool includeNegotiation,
    const std::uint8_t* userInformation,
    std::size_t userInformationSize,
    std::vector<std::uint8_t>& output);
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
  std::uint8_t acknowledgeSequence_;
  bool connectResponsePending_;
  bool disconnectResponsePending_;
  bool dmResponsePending_;

  // Negotiation parameters pending until BuildConnectResponse is called (server).
  struct PendingNegotiation
  {
    bool active;
    std::size_t maxInfoTx;
    std::size_t maxInfoRx;
    std::uint32_t windowTx;
    std::uint32_t windowRx;
  };
  PendingNegotiation pendingNegotiation_;

  HdlcSessionNegotiationLimits negotiatedLimits_;
  std::uint8_t windowSizeTransmit_;

  std::vector<std::uint8_t> receivedUserInformation_;
};

} // namespace hdlc
} // namespace dlms
