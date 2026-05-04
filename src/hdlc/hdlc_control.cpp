#include "dlms/hdlc/hdlc_control.hpp"

namespace dlms {
namespace hdlc {

namespace {

bool IsSupportedUnnumbered(std::uint8_t value)
{
  switch (value & 0xefu) {
    case 0x83u: // SNRM
    case 0x63u: // UA
    case 0x43u: // DISC
    case 0x0fu: // DM
    case 0x87u: // FRMR
    case 0x03u: // UI
      return true;
    default:
      return false;
  }
}

} // namespace

HdlcControl::HdlcControl()
  : rawValue_(0u)
{
}

HdlcStatus HdlcControl::Decode(
  std::uint8_t value,
  HdlcControl& control)
{
  if ((value & 0x01u) == 0u) {
    control.rawValue_ = value;
    return HdlcStatus::Ok;
  }

  if ((value & 0x03u) == 0x01u) {
    control.rawValue_ = value;
    return HdlcStatus::Ok;
  }

  if (IsSupportedUnnumbered(value)) {
    control.rawValue_ = value;
    return HdlcStatus::Ok;
  }

  return HdlcStatus::InvalidControlField;
}

HdlcStatus HdlcControl::Encode(std::uint8_t& value) const
{
  value = rawValue_;
  return HdlcStatus::Ok;
}

HdlcFrameKind HdlcControl::FrameKind() const
{
  if ((rawValue_ & 0x01u) == 0u) {
    return HdlcFrameKind::Information;
  }

  if ((rawValue_ & 0x03u) == 0x01u) {
    return HdlcFrameKind::Supervisory;
  }

  return HdlcFrameKind::Unnumbered;
}

bool HdlcControl::PollFinal() const
{
  return (rawValue_ & 0x10u) != 0u;
}

HdlcStatus HdlcControl::SupervisoryKind(HdlcSupervisoryKind& kind) const
{
  if (FrameKind() != HdlcFrameKind::Supervisory) {
    return HdlcStatus::InvalidFrameType;
  }

  switch ((rawValue_ >> 2) & 0x03u) {
    case 0u:
      kind = HdlcSupervisoryKind::ReceiveReady;
      return HdlcStatus::Ok;
    case 1u:
      kind = HdlcSupervisoryKind::ReceiveNotReady;
      return HdlcStatus::Ok;
    case 2u:
      kind = HdlcSupervisoryKind::Reject;
      return HdlcStatus::Ok;
    case 3u:
      kind = HdlcSupervisoryKind::SelectiveReject;
      return HdlcStatus::Ok;
    default:
      return HdlcStatus::InvalidControlField;
  }
}

HdlcStatus HdlcControl::UnnumberedKind(HdlcUnnumberedKind& kind) const
{
  if (FrameKind() != HdlcFrameKind::Unnumbered) {
    return HdlcStatus::InvalidFrameType;
  }

  switch (rawValue_ & 0xefu) {
    case 0x83u:
      kind = HdlcUnnumberedKind::Snrm;
      return HdlcStatus::Ok;
    case 0x63u:
      kind = HdlcUnnumberedKind::Ua;
      return HdlcStatus::Ok;
    case 0x43u:
      kind = HdlcUnnumberedKind::Disc;
      return HdlcStatus::Ok;
    case 0x0fu:
      kind = HdlcUnnumberedKind::Dm;
      return HdlcStatus::Ok;
    case 0x87u:
      kind = HdlcUnnumberedKind::Frmr;
      return HdlcStatus::Ok;
    case 0x03u:
      kind = HdlcUnnumberedKind::Ui;
      return HdlcStatus::Ok;
    default:
      return HdlcStatus::InvalidControlField;
  }
}

std::uint8_t HdlcControl::SendSequence() const
{
  if (FrameKind() != HdlcFrameKind::Information) {
    return 0u;
  }

  return static_cast<std::uint8_t>((rawValue_ >> 1) & 0x07u);
}

std::uint8_t HdlcControl::ReceiveSequence() const
{
  if (FrameKind() == HdlcFrameKind::Unnumbered) {
    return 0u;
  }

  return static_cast<std::uint8_t>((rawValue_ >> 5) & 0x07u);
}

} // namespace hdlc
} // namespace dlms
