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
