#include "dlms/hdlc/hdlc_session.hpp"
#include "dlms/hdlc/hdlc_codec.hpp"

#include <algorithm>

namespace dlms {
namespace hdlc {

namespace {

const std::uint8_t kSnrmControl = 0x93u;
const std::uint8_t kUaControl   = 0x73u;
const std::uint8_t kDiscControl = 0x53u;
const std::uint8_t kDmControl   = 0x1fu;  // DM with F=1
const std::uint8_t kReceiveReadyBase = 0x01u;

const std::size_t kDefaultMaxInfoFieldLength = 128u;
const std::uint8_t kDefaultWindowSize = 1u;

std::uint8_t NextSequence(std::uint8_t sequence)
{
  return static_cast<std::uint8_t>((sequence + 1u) & 0x07u);
}

bool SameAddress(const HdlcAddress& left, const HdlcAddress& right)
{
  return left.RawValue() == right.RawValue() &&
         left.EncodedSize() == right.EncodedSize();
}

std::uint8_t MakeInformationControl(
  std::uint8_t sendSequence,
  std::uint8_t receiveSequence,
  bool pollFinal)
{
  return static_cast<std::uint8_t>(
    ((sendSequence & 0x07u) << 1) |
    (pollFinal ? 0x10u : 0x00u) |
    ((receiveSequence & 0x07u) << 5));
}

std::uint8_t MakeReceiveReadyControl(
  std::uint8_t receiveSequence,
  bool pollFinal)
{
  return static_cast<std::uint8_t>(
    kReceiveReadyBase |
    (pollFinal ? 0x10u : 0x00u) |
    ((receiveSequence & 0x07u) << 5));
}

// --------------------------------------------------------------------------
// SNRM/UA parameter TLV encoding and parsing
// --------------------------------------------------------------------------

struct SnrmParams
{
  std::size_t maxInfoTx;   // tag 05h: max info the primary transmits
  std::size_t maxInfoRx;   // tag 06h: max info the primary receives
  std::uint32_t windowTx;  // tag 07h: window the primary uses for transmit
  std::uint32_t windowRx;  // tag 08h: window the primary uses for receive
};

SnrmParams DefaultSnrmParams()
{
  SnrmParams p;
  p.maxInfoTx = kDefaultMaxInfoFieldLength;
  p.maxInfoRx = kDefaultMaxInfoFieldLength;
  p.windowTx  = kDefaultWindowSize;
  p.windowRx  = kDefaultWindowSize;
  return p;
}

std::uint32_t ParseBeValue(const std::uint8_t* data, std::size_t len)
{
  std::uint32_t result = 0u;
  for (std::size_t i = 0u; i < len; ++i) {
    result = (result << 8) | data[i];
  }
  return result;
}

// Parse a SNRM/UA Information field.
//   On success: params holds the parsed values, negotiationConsumed is set to
//   the number of bytes belonging to the 81-80-len block (0 if absent).
//   Returns UnsupportedFeature for unrecognised parameter tags.
HdlcStatus ParseSnrmInfoField(
  const std::uint8_t* data,
  std::size_t size,
  SnrmParams& params,
  std::size_t& negotiationConsumed)
{
  params = DefaultSnrmParams();
  negotiationConsumed = 0u;

  if (size == 0u) {
    return HdlcStatus::Ok;
  }

  // Check for negotiation block header 81h 80h <len>
  if (size < 3u || data[0] != 0x81u || data[1] != 0x80u) {
    return HdlcStatus::Ok;  // no negotiation block; all bytes are user_information
  }

  const std::size_t groupLen = data[2];
  if (3u + groupLen > size) {
    return HdlcStatus::InvalidFrameFormat;
  }

  negotiationConsumed = 3u + groupLen;
  std::size_t pos = 3u;
  const std::size_t end = 3u + groupLen;

  while (pos < end) {
    if (pos + 2u > end) {
      return HdlcStatus::InvalidFrameFormat;
    }
    const std::uint8_t tag = data[pos++];
    const std::size_t valLen = data[pos++];

    if (valLen == 0u || valLen > 4u || pos + valLen > end) {
      return HdlcStatus::InvalidFrameFormat;
    }

    const std::uint32_t value = ParseBeValue(data + pos, valLen);
    pos += valLen;

    switch (tag) {
      case 0x05u: params.maxInfoTx = value; break;
      case 0x06u: params.maxInfoRx = value; break;
      case 0x07u: params.windowTx  = value; break;
      case 0x08u: params.windowRx  = value; break;
      default:    return HdlcStatus::UnsupportedFeature;
    }
  }

  return HdlcStatus::Ok;
}

// Encode all four SNRM/UA negotiation parameters into the 81-80-len block.
// Always writes 23 bytes: 3-byte header + 20-byte group.
void EncodeSnrmNegotiationBlock(
  const SnrmParams& params,
  std::vector<std::uint8_t>& output)
{
  // Group length = 4 params: 4+4+6+6 = 20 bytes
  output.push_back(0x81u);
  output.push_back(0x80u);
  output.push_back(0x14u);  // 20

  // Tag 05h: max_info_tx — 2-byte big-endian value
  output.push_back(0x05u);
  output.push_back(0x02u);
  output.push_back(static_cast<std::uint8_t>((params.maxInfoTx >> 8) & 0xffu));
  output.push_back(static_cast<std::uint8_t>( params.maxInfoTx       & 0xffu));

  // Tag 06h: max_info_rx — 2-byte big-endian value
  output.push_back(0x06u);
  output.push_back(0x02u);
  output.push_back(static_cast<std::uint8_t>((params.maxInfoRx >> 8) & 0xffu));
  output.push_back(static_cast<std::uint8_t>( params.maxInfoRx       & 0xffu));

  // Tag 07h: window_tx — 4-byte big-endian value
  output.push_back(0x07u);
  output.push_back(0x04u);
  output.push_back(0x00u);
  output.push_back(0x00u);
  output.push_back(0x00u);
  output.push_back(static_cast<std::uint8_t>(params.windowTx & 0xffu));

  // Tag 08h: window_rx — 4-byte big-endian value
  output.push_back(0x08u);
  output.push_back(0x04u);
  output.push_back(0x00u);
  output.push_back(0x00u);
  output.push_back(0x00u);
  output.push_back(static_cast<std::uint8_t>(params.windowRx & 0xffu));
}

// Apply the negotiation rule: result = min(proposed, local_capability).
// Tags 05h/07h are bounded by the local receive capability.
// Tags 06h/08h are bounded by the local transmit capability.
SnrmParams NegotiateParams(
  const SnrmParams& proposed,
  const HdlcSessionNegotiationLimits& local)
{
  SnrmParams n;
  n.maxInfoTx = std::min(proposed.maxInfoTx, local.maxInformationFieldLengthReceive);
  n.maxInfoRx = std::min(proposed.maxInfoRx, local.maxInformationFieldLengthTransmit);
  n.windowTx  = std::min(proposed.windowTx,
                         static_cast<std::uint32_t>(local.windowSizeReceive));
  n.windowRx  = std::min(proposed.windowRx,
                         static_cast<std::uint32_t>(local.windowSizeTransmit));
  return n;
}

bool IsDefaultNegotiationLimits(const HdlcSessionNegotiationLimits& l)
{
  return l.maxInformationFieldLengthTransmit == kDefaultMaxInfoFieldLength &&
         l.maxInformationFieldLengthReceive  == kDefaultMaxInfoFieldLength &&
         l.windowSizeTransmit == kDefaultWindowSize &&
         l.windowSizeReceive  == kDefaultWindowSize;
}

} // namespace

// --------------------------------------------------------------------------

HdlcSessionNegotiationLimits DefaultHdlcSessionNegotiationLimits()
{
  HdlcSessionNegotiationLimits l;
  l.maxInformationFieldLengthTransmit = kDefaultMaxInfoFieldLength;
  l.maxInformationFieldLengthReceive  = kDefaultMaxInfoFieldLength;
  l.windowSizeTransmit = kDefaultWindowSize;
  l.windowSizeReceive  = kDefaultWindowSize;
  return l;
}

HdlcSession::HdlcSession(const HdlcSessionOptions& options)
  : options_(options),
    state_(HdlcSessionState::Disconnected),
    sendSequence_(0u),
    receiveSequence_(0u),
    acknowledgeSequence_(0u),
    connectResponsePending_(false),
    disconnectResponsePending_(false),
    dmResponsePending_(false)
{
  // Normalise zero fields to protocol defaults.
  if (options_.negotiationLimits.maxInformationFieldLengthTransmit == 0u) {
    options_.negotiationLimits.maxInformationFieldLengthTransmit =
      kDefaultMaxInfoFieldLength;
  }
  if (options_.negotiationLimits.maxInformationFieldLengthReceive == 0u) {
    options_.negotiationLimits.maxInformationFieldLengthReceive =
      kDefaultMaxInfoFieldLength;
  }
  if (options_.negotiationLimits.windowSizeTransmit == 0u) {
    options_.negotiationLimits.windowSizeTransmit = kDefaultWindowSize;
  }
  if (options_.negotiationLimits.windowSizeReceive == 0u) {
    options_.negotiationLimits.windowSizeReceive = kDefaultWindowSize;
  }

  pendingNegotiation_.active    = false;
  pendingNegotiation_.maxInfoTx = kDefaultMaxInfoFieldLength;
  pendingNegotiation_.maxInfoRx = kDefaultMaxInfoFieldLength;
  pendingNegotiation_.windowTx  = kDefaultWindowSize;
  pendingNegotiation_.windowRx  = kDefaultWindowSize;

  negotiatedLimits_ = options_.negotiationLimits;
  windowSizeTransmit_ = options_.negotiationLimits.windowSizeTransmit;
}

HdlcSessionState HdlcSession::State() const
{
  return state_;
}

std::uint8_t HdlcSession::SendSequence() const
{
  return sendSequence_;
}

std::uint8_t HdlcSession::ReceiveSequence() const
{
  return receiveSequence_;
}

std::uint8_t HdlcSession::AcknowledgeSequence() const
{
  return acknowledgeSequence_;
}

bool HdlcSession::CanSendInformationFrame() const
{
  const std::uint8_t outstanding =
    static_cast<std::uint8_t>((sendSequence_ - acknowledgeSequence_) & 0x07u);
  return outstanding < windowSizeTransmit_;
}

const HdlcSessionNegotiationLimits& HdlcSession::NegotiatedLimits() const
{
  return negotiatedLimits_;
}

const std::vector<std::uint8_t>& HdlcSession::ReceivedUserInformation() const
{
  return receivedUserInformation_;
}

// --------------------------------------------------------------------------
// Build methods
// --------------------------------------------------------------------------

HdlcStatus HdlcSession::BuildConnectRequest(std::vector<std::uint8_t>& output)
{
  const bool includeNeg = !IsDefaultNegotiationLimits(options_.negotiationLimits);
  return BuildConnectRequestInternal(includeNeg, 0, 0u, output);
}

HdlcStatus HdlcSession::BuildConnectRequest(
  const std::uint8_t* userInformation,
  std::size_t userInformationSize,
  std::vector<std::uint8_t>& output)
{
  return BuildConnectRequestInternal(true, userInformation, userInformationSize, output);
}

HdlcStatus HdlcSession::BuildConnectRequestInternal(
  bool includeNegotiation,
  const std::uint8_t* userInformation,
  std::size_t userInformationSize,
  std::vector<std::uint8_t>& output)
{
  output.clear();

  if (options_.role != HdlcSessionRole::Client) {
    return HdlcStatus::UnsupportedFeature;
  }

  if (state_ != HdlcSessionState::Disconnected) {
    return HdlcStatus::UnsupportedFrame;
  }

  // Build info field when negotiation params or user_information are present.
  std::vector<std::uint8_t> infoField;
  const bool hasUserInfo =
    (userInformation != 0) && (userInformationSize > 0u);

  if (includeNegotiation || hasUserInfo) {
    if (includeNegotiation) {
      SnrmParams proposed;
      proposed.maxInfoTx = options_.negotiationLimits.maxInformationFieldLengthTransmit;
      proposed.maxInfoRx = options_.negotiationLimits.maxInformationFieldLengthReceive;
      proposed.windowTx  = options_.negotiationLimits.windowSizeTransmit;
      proposed.windowRx  = options_.negotiationLimits.windowSizeReceive;
      EncodeSnrmNegotiationBlock(proposed, infoField);
    }
    if (hasUserInfo) {
      infoField.insert(
        infoField.end(),
        userInformation,
        userInformation + userInformationSize);
    }
  }

  const std::uint8_t* infoData = infoField.empty() ? 0 : infoField.data();
  const HdlcStatus status =
    BuildFrame(kSnrmControl, infoData, infoField.size(), output);
  if (status == HdlcStatus::Ok) {
    state_               = HdlcSessionState::AwaitingConnection;
    sendSequence_        = 0u;
    receiveSequence_     = 0u;
    acknowledgeSequence_ = 0u;
  }
  return status;
}

HdlcStatus HdlcSession::BuildConnectResponse(std::vector<std::uint8_t>& output)
{
  output.clear();

  if (options_.role != HdlcSessionRole::Server) {
    return HdlcStatus::UnsupportedFeature;
  }

  if (!connectResponsePending_) {
    return HdlcStatus::UnsupportedFrame;
  }

  // Compute negotiated params and build the UA info field when the SNRM
  // included negotiation parameters.
  std::vector<std::uint8_t> infoField;
  if (pendingNegotiation_.active) {
    SnrmParams proposed;
    proposed.maxInfoTx = pendingNegotiation_.maxInfoTx;
    proposed.maxInfoRx = pendingNegotiation_.maxInfoRx;
    proposed.windowTx  = pendingNegotiation_.windowTx;
    proposed.windowRx  = pendingNegotiation_.windowRx;

    const SnrmParams negotiated =
      NegotiateParams(proposed, options_.negotiationLimits);

    EncodeSnrmNegotiationBlock(negotiated, infoField);

    // Update server-side negotiated limits:
    //   server transmits up to negotiated.maxInfoRx (tag 06h)
    //   server receives up to negotiated.maxInfoTx  (tag 05h)
    negotiatedLimits_.maxInformationFieldLengthTransmit = negotiated.maxInfoRx;
    negotiatedLimits_.maxInformationFieldLengthReceive  = negotiated.maxInfoTx;
    negotiatedLimits_.windowSizeTransmit =
      static_cast<std::uint8_t>(negotiated.windowRx);
    negotiatedLimits_.windowSizeReceive =
      static_cast<std::uint8_t>(negotiated.windowTx);

    // Update codec limits for outgoing frame sizing.
    options_.limits.maximumInformationFieldSize =
      negotiatedLimits_.maxInformationFieldLengthTransmit;
    windowSizeTransmit_ = negotiatedLimits_.windowSizeTransmit;
  }

  const std::uint8_t* infoData = infoField.empty() ? 0 : infoField.data();
  const HdlcStatus status =
    BuildFrame(kUaControl, infoData, infoField.size(), output);
  if (status == HdlcStatus::Ok) {
    state_                     = HdlcSessionState::Connected;
    connectResponsePending_    = false;
    pendingNegotiation_.active = false;
    sendSequence_              = 0u;
    receiveSequence_           = 0u;
    acknowledgeSequence_       = 0u;
  }
  return status;
}

HdlcStatus HdlcSession::BuildDisconnectRequest(
  std::vector<std::uint8_t>& output)
{
  return BuildDisconnectRequest(0, 0u, output);
}

HdlcStatus HdlcSession::BuildDisconnectRequest(
  const std::uint8_t* userInformation,
  std::size_t userInformationSize,
  std::vector<std::uint8_t>& output)
{
  output.clear();

  if (state_ != HdlcSessionState::Connected) {
    return HdlcStatus::UnsupportedFrame;
  }

  const bool hasUserInfo =
    (userInformation != 0) && (userInformationSize > 0u);
  const std::uint8_t* infoData = hasUserInfo ? userInformation : 0;
  const std::size_t infoSize   = hasUserInfo ? userInformationSize : 0u;

  const HdlcStatus status = BuildFrame(kDiscControl, infoData, infoSize, output);
  if (status == HdlcStatus::Ok) {
    state_ = HdlcSessionState::AwaitingDisconnect;
  }
  return status;
}

HdlcStatus HdlcSession::BuildDisconnectResponse(
  std::vector<std::uint8_t>& output)
{
  output.clear();

  if (!disconnectResponsePending_) {
    return HdlcStatus::UnsupportedFrame;
  }

  const HdlcStatus status = BuildUnnumberedFrame(kUaControl, output);
  if (status == HdlcStatus::Ok) {
    state_                       = HdlcSessionState::Disconnected;
    disconnectResponsePending_   = false;
    sendSequence_                = 0u;
    receiveSequence_             = 0u;
    acknowledgeSequence_         = 0u;
  }
  return status;
}

HdlcStatus HdlcSession::BuildDmResponse(std::vector<std::uint8_t>& output)
{
  output.clear();

  if (!dmResponsePending_) {
    return HdlcStatus::UnsupportedFrame;
  }

  const HdlcStatus status = BuildUnnumberedFrame(kDmControl, output);
  if (status == HdlcStatus::Ok) {
    dmResponsePending_ = false;
  }
  return status;
}

HdlcStatus HdlcSession::BuildInformationFrame(
  const std::uint8_t* information,
  std::size_t informationSize,
  bool pollFinal,
  std::vector<std::uint8_t>& output)
{
  output.clear();

  if (state_ != HdlcSessionState::Connected) {
    return HdlcStatus::UnsupportedFrame;
  }

  if (!CanSendInformationFrame()) {
    return HdlcStatus::UnsupportedFrame;
  }

  const std::uint8_t control =
    MakeInformationControl(sendSequence_, receiveSequence_, pollFinal);
  const HdlcStatus status =
    BuildFrame(control, information, informationSize, output);
  if (status == HdlcStatus::Ok) {
    sendSequence_ = NextSequence(sendSequence_);
  }
  return status;
}

HdlcStatus HdlcSession::BuildReceiveReadyFrame(
  bool pollFinal,
  std::vector<std::uint8_t>& output)
{
  output.clear();

  if (state_ != HdlcSessionState::Connected) {
    return HdlcStatus::UnsupportedFrame;
  }

  return BuildSupervisoryFrame(
    MakeReceiveReadyControl(receiveSequence_, pollFinal),
    output);
}

HdlcStatus HdlcSession::ReceiveFrame(const HdlcFrameBuffer& frame)
{
  if (!IncomingAddressesMatch(frame)) {
    return HdlcStatus::InvalidAddress;
  }

  switch (frame.control.FrameKind()) {
    case HdlcFrameKind::Information:
      return ReceiveInformationFrame(frame);
    case HdlcFrameKind::Supervisory:
      return ReceiveSupervisoryFrame(frame);
    case HdlcFrameKind::Unnumbered:
      return ReceiveUnnumberedFrame(frame);
    default:
      return HdlcStatus::InvalidFrameType;
  }
}

// --------------------------------------------------------------------------
// Private helpers
// --------------------------------------------------------------------------

HdlcStatus HdlcSession::BuildUnnumberedFrame(
  std::uint8_t control,
  std::vector<std::uint8_t>& output) const
{
  return BuildFrame(control, 0, 0u, output);
}

HdlcStatus HdlcSession::BuildSupervisoryFrame(
  std::uint8_t control,
  std::vector<std::uint8_t>& output) const
{
  return BuildFrame(control, 0, 0u, output);
}

HdlcStatus HdlcSession::BuildFrame(
  std::uint8_t control,
  const std::uint8_t* information,
  std::size_t informationSize,
  std::vector<std::uint8_t>& output) const
{
  HdlcFrame frame;
  frame.segmented       = false;
  frame.destination     = RemoteAddress();
  frame.source          = LocalAddress();
  frame.informationData = information;
  frame.informationSize = informationSize;

  HdlcStatus status = HdlcControl::Decode(control, frame.control);
  if (status != HdlcStatus::Ok) {
    return status;
  }

  return EncodeFrame(frame, options_.limits, output);
}

HdlcStatus HdlcSession::ReceiveInformationFrame(const HdlcFrameBuffer& frame)
{
  if (state_ != HdlcSessionState::Connected) {
    return HdlcStatus::UnsupportedFrame;
  }

  if (frame.control.SendSequence() != receiveSequence_) {
    return HdlcStatus::InvalidControlField;
  }

  // Validate piggybacked N(R): must be within [V(A), V(S)] mod 8.
  const std::uint8_t nr = frame.control.ReceiveSequence();
  const std::uint8_t ackToNr =
    static_cast<std::uint8_t>((nr - acknowledgeSequence_) & 0x07u);
  const std::uint8_t ackToSend =
    static_cast<std::uint8_t>((sendSequence_ - acknowledgeSequence_) & 0x07u);
  if (ackToNr > ackToSend) {
    return HdlcStatus::InvalidControlField;
  }

  acknowledgeSequence_ = nr;
  receiveSequence_     = NextSequence(receiveSequence_);
  return HdlcStatus::Ok;
}

HdlcStatus HdlcSession::ReceiveSupervisoryFrame(const HdlcFrameBuffer& frame)
{
  if (state_ != HdlcSessionState::Connected) {
    return HdlcStatus::UnsupportedFrame;
  }

  HdlcSupervisoryKind kind;
  HdlcStatus status = frame.control.SupervisoryKind(kind);
  if (status != HdlcStatus::Ok) {
    return status;
  }
  (void)kind;

  // Validate N(R): must be within [V(A), V(S)] mod 8.
  const std::uint8_t nr = frame.control.ReceiveSequence();
  const std::uint8_t ackToNr =
    static_cast<std::uint8_t>((nr - acknowledgeSequence_) & 0x07u);
  const std::uint8_t ackToSend =
    static_cast<std::uint8_t>((sendSequence_ - acknowledgeSequence_) & 0x07u);
  if (ackToNr > ackToSend) {
    return HdlcStatus::InvalidControlField;
  }

  acknowledgeSequence_ = nr;
  return HdlcStatus::Ok;
}

HdlcStatus HdlcSession::ReceiveUnnumberedFrame(const HdlcFrameBuffer& frame)
{
  HdlcUnnumberedKind kind;
  HdlcStatus status = frame.control.UnnumberedKind(kind);
  if (status != HdlcStatus::Ok) {
    return status;
  }

  if (kind == HdlcUnnumberedKind::Snrm) {
    if (options_.role != HdlcSessionRole::Server ||
        state_ != HdlcSessionState::Disconnected) {
      return HdlcStatus::UnsupportedFrame;
    }

    // Parse info field for negotiation params and user_information.
    receivedUserInformation_.clear();
    pendingNegotiation_.active = false;

    if (!frame.information.empty()) {
      SnrmParams proposed;
      std::size_t negotiationConsumed = 0u;
      const HdlcStatus parseStatus = ParseSnrmInfoField(
        frame.information.data(),
        frame.information.size(),
        proposed,
        negotiationConsumed);

      if (parseStatus != HdlcStatus::Ok) {
        // Unknown or malformed parameter: reject with DM.
        dmResponsePending_ = true;
        return parseStatus;
      }

      if (negotiationConsumed > 0u) {
        pendingNegotiation_.active    = true;
        pendingNegotiation_.maxInfoTx = proposed.maxInfoTx;
        pendingNegotiation_.maxInfoRx = proposed.maxInfoRx;
        pendingNegotiation_.windowTx  = proposed.windowTx;
        pendingNegotiation_.windowRx  = proposed.windowRx;
      }

      if (negotiationConsumed < frame.information.size()) {
        receivedUserInformation_.assign(
          frame.information.begin() +
            static_cast<std::ptrdiff_t>(negotiationConsumed),
          frame.information.end());
      }
    }

    connectResponsePending_ = true;
    sendSequence_           = 0u;
    receiveSequence_        = 0u;
    acknowledgeSequence_    = 0u;
    return HdlcStatus::Ok;
  }

  if (kind == HdlcUnnumberedKind::Ua) {
    if (state_ == HdlcSessionState::AwaitingConnection) {
      // Parse UA info field for negotiated params (client side).
      if (!frame.information.empty()) {
        SnrmParams uaParams;
        std::size_t consumed = 0u;
        if (ParseSnrmInfoField(
              frame.information.data(),
              frame.information.size(),
              uaParams,
              consumed) == HdlcStatus::Ok &&
            consumed > 0u) {
          // From the client's perspective:
          //   tag 05h = max info client transmits (server confirmed receive)
          //   tag 06h = max info client can receive from server
          negotiatedLimits_.maxInformationFieldLengthTransmit = uaParams.maxInfoTx;
          negotiatedLimits_.maxInformationFieldLengthReceive  = uaParams.maxInfoRx;
          negotiatedLimits_.windowSizeTransmit =
            static_cast<std::uint8_t>(uaParams.windowTx);
          negotiatedLimits_.windowSizeReceive =
            static_cast<std::uint8_t>(uaParams.windowRx);

          options_.limits.maximumInformationFieldSize =
            negotiatedLimits_.maxInformationFieldLengthTransmit;
          windowSizeTransmit_ = negotiatedLimits_.windowSizeTransmit;
        }
      }

      state_               = HdlcSessionState::Connected;
      sendSequence_        = 0u;
      receiveSequence_     = 0u;
      acknowledgeSequence_ = 0u;
      return HdlcStatus::Ok;
    }

    if (state_ == HdlcSessionState::AwaitingDisconnect) {
      state_               = HdlcSessionState::Disconnected;
      sendSequence_        = 0u;
      receiveSequence_     = 0u;
      acknowledgeSequence_ = 0u;
      return HdlcStatus::Ok;
    }

    return HdlcStatus::UnsupportedFrame;
  }

  if (kind == HdlcUnnumberedKind::Disc) {
    if (state_ != HdlcSessionState::Connected) {
      return HdlcStatus::UnsupportedFrame;
    }

    receivedUserInformation_.clear();
    if (!frame.information.empty()) {
      receivedUserInformation_ = frame.information;
    }

    disconnectResponsePending_ = true;
    return HdlcStatus::Ok;
  }

  if (kind == HdlcUnnumberedKind::Dm) {
    state_               = HdlcSessionState::Disconnected;
    sendSequence_        = 0u;
    receiveSequence_     = 0u;
    acknowledgeSequence_ = 0u;
    return HdlcStatus::Ok;
  }

  return HdlcStatus::UnsupportedFrame;
}

bool HdlcSession::IncomingAddressesMatch(const HdlcFrameBuffer& frame) const
{
  return SameAddress(frame.destination, LocalAddress()) &&
         SameAddress(frame.source, RemoteAddress());
}

const HdlcAddress& HdlcSession::LocalAddress() const
{
  return options_.role == HdlcSessionRole::Client ?
    options_.clientAddress :
    options_.serverAddress;
}

const HdlcAddress& HdlcSession::RemoteAddress() const
{
  return options_.role == HdlcSessionRole::Client ?
    options_.serverAddress :
    options_.clientAddress;
}

} // namespace hdlc
} // namespace dlms
