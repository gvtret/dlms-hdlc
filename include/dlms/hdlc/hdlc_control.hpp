#pragma once

#include "dlms/hdlc/hdlc_error.hpp"

#include <cstdint>

namespace dlms {
namespace hdlc {

/**
 * @brief High-level category encoded by the HDLC control field.
 */
enum class HdlcFrameKind
{
  /// Information frame carrying user information and sequence numbers.
  Information,
  /// Supervisory frame such as RR, RNR, REJ, or SREJ.
  Supervisory,
  /// Unnumbered frame such as SNRM, UA, DISC, DM, FRMR, or UI.
  Unnumbered
};

/**
 * @brief Supported HDLC supervisory command/response kind.
 */
enum class HdlcSupervisoryKind
{
  /// Receive Ready.
  ReceiveReady,
  /// Receive Not Ready.
  ReceiveNotReady,
  /// Reject.
  Reject,
  /// Selective Reject.
  SelectiveReject
};

/**
 * @brief Supported HDLC unnumbered command/response kind.
 */
enum class HdlcUnnumberedKind
{
  /// Set Normal Response Mode.
  Snrm,
  /// Unnumbered Acknowledgement.
  Ua,
  /// Disconnect.
  Disc,
  /// Disconnected Mode.
  Dm,
  /// Frame Reject.
  Frmr,
  /// Unnumbered Information.
  Ui
};

/**
 * @brief Parsed HDLC control field.
 *
 * The class preserves the raw control byte and exposes frame kind,
 * Poll/Final bit, and sequence-number accessors for I/S frames.
 */
class HdlcControl
{
public:
  /**
   * @brief Construct a default control field.
   *
   * The default object is intended as a decode target and should be replaced
   * with `Decode` before encoding into a frame.
   */
  HdlcControl();

  /**
   * @brief Decode a raw HDLC control byte.
   *
   * @param value Wire control byte.
   * @param control Receives the parsed control object on success.
   * @return `Ok` or `InvalidControlField`/`InvalidFrameType`.
   */
  static HdlcStatus Decode(
    std::uint8_t value,
    HdlcControl& control);

  /**
   * @brief Encode this control object back to the raw wire byte.
   *
   * @param value Receives the encoded control byte.
   * @return `Ok` for supported control objects.
   */
  HdlcStatus Encode(std::uint8_t& value) const;

  /**
   * @brief Return the high-level HDLC frame category.
   * @return Information, supervisory, or unnumbered frame kind.
   */
  HdlcFrameKind FrameKind() const;

  /**
   * @brief Return the Poll/Final bit from the control field.
   * @return `true` when the P/F bit is set.
   */
  bool PollFinal() const;

  /**
   * @brief Return the supervisory command kind.
   *
   * @param kind Receives the decoded kind for supervisory frames.
   * @return `Ok` for supervisory frames, otherwise `InvalidFrameType`.
   */
  HdlcStatus SupervisoryKind(HdlcSupervisoryKind& kind) const;

  /**
   * @brief Return the unnumbered command/response kind.
   *
   * @param kind Receives the decoded kind for unnumbered frames.
   * @return `Ok` for supported unnumbered frames, otherwise
   * `InvalidFrameType`.
   */
  HdlcStatus UnnumberedKind(HdlcUnnumberedKind& kind) const;

  /**
   * @brief Return the send sequence number `N(S)` for I-frames.
   * @return Three-bit send sequence value; zero for frame types without `N(S)`.
   */
  std::uint8_t SendSequence() const;
  /**
   * @brief Return the receive sequence number `N(R)`.
   * @return Three-bit receive sequence value for I/S frames.
   */
  std::uint8_t ReceiveSequence() const;

private:
  std::uint8_t rawValue_;
};

} // namespace hdlc
} // namespace dlms
