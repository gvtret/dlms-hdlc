#include "dlms/hdlc/hdlc_session.hpp"

namespace dlms {
namespace hdlc {

namespace {

const std::uint8_t kSnrmControl = 0x93u;
const std::uint8_t kUaControl = 0x73u;
const std::uint8_t kDiscControl = 0x53u;
const std::uint8_t kReceiveReadyBase = 0x01u;

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

} // namespace

HdlcSession::HdlcSession(const HdlcSessionOptions& options)
  : options_(options),
    state_(HdlcSessionState::Disconnected),
    sendSequence_(0u),
    receiveSequence_(0u),
    connectResponsePending_(false),
    disconnectResponsePending_(false)
{
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

HdlcStatus HdlcSession::BuildConnectRequest(std::vector<std::uint8_t>& output)
{
  output.clear();

  if (options_.role != HdlcSessionRole::Client) {
    return HdlcStatus::UnsupportedFeature;
  }

  if (state_ != HdlcSessionState::Disconnected) {
    return HdlcStatus::UnsupportedFrame;
  }

  const HdlcStatus status = BuildUnnumberedFrame(kSnrmControl, output);
  if (status == HdlcStatus::Ok) {
    state_ = HdlcSessionState::AwaitingConnection;
    sendSequence_ = 0u;
    receiveSequence_ = 0u;
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

  const HdlcStatus status = BuildUnnumberedFrame(kUaControl, output);
  if (status == HdlcStatus::Ok) {
    state_ = HdlcSessionState::Connected;
    connectResponsePending_ = false;
    sendSequence_ = 0u;
    receiveSequence_ = 0u;
  }
  return status;
}

HdlcStatus HdlcSession::BuildDisconnectRequest(
  std::vector<std::uint8_t>& output)
{
  output.clear();

  if (state_ != HdlcSessionState::Connected) {
    return HdlcStatus::UnsupportedFrame;
  }

  const HdlcStatus status = BuildUnnumberedFrame(kDiscControl, output);
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
    state_ = HdlcSessionState::Disconnected;
    disconnectResponsePending_ = false;
    sendSequence_ = 0u;
    receiveSequence_ = 0u;
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
  frame.segmented = false;
  frame.destination = RemoteAddress();
  frame.source = LocalAddress();
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

  if (frame.control.ReceiveSequence() != sendSequence_) {
    return HdlcStatus::InvalidControlField;
  }

  receiveSequence_ = NextSequence(receiveSequence_);
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

  if (frame.control.ReceiveSequence() != sendSequence_) {
    return HdlcStatus::InvalidControlField;
  }

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

    connectResponsePending_ = true;
    sendSequence_ = 0u;
    receiveSequence_ = 0u;
    return HdlcStatus::Ok;
  }

  if (kind == HdlcUnnumberedKind::Ua) {
    if (state_ == HdlcSessionState::AwaitingConnection) {
      state_ = HdlcSessionState::Connected;
      sendSequence_ = 0u;
      receiveSequence_ = 0u;
      return HdlcStatus::Ok;
    }

    if (state_ == HdlcSessionState::AwaitingDisconnect) {
      state_ = HdlcSessionState::Disconnected;
      sendSequence_ = 0u;
      receiveSequence_ = 0u;
      return HdlcStatus::Ok;
    }

    return HdlcStatus::UnsupportedFrame;
  }

  if (kind == HdlcUnnumberedKind::Disc) {
    if (state_ != HdlcSessionState::Connected) {
      return HdlcStatus::UnsupportedFrame;
    }

    disconnectResponsePending_ = true;
    return HdlcStatus::Ok;
  }

  if (kind == HdlcUnnumberedKind::Dm) {
    state_ = HdlcSessionState::Disconnected;
    sendSequence_ = 0u;
    receiveSequence_ = 0u;
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
