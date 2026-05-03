#pragma once

#include "dlms/hdlc/hdlc_error.hpp"

#include <cstdint>

namespace dlms {
namespace hdlc {

enum class HdlcFrameKind
{
  Information,
  Supervisory,
  Unnumbered
};

enum class HdlcSupervisoryKind
{
  ReceiveReady,
  ReceiveNotReady,
  Reject,
  SelectiveReject
};

enum class HdlcUnnumberedKind
{
  Snrm,
  Ua,
  Disc,
  Dm,
  Frmr,
  Ui
};

class HdlcControl
{
public:
  HdlcControl();

  static HdlcStatus Decode(
    std::uint8_t value,
    HdlcControl& control);

  HdlcStatus Encode(std::uint8_t& value) const;

  HdlcFrameKind FrameKind() const;

  bool PollFinal() const;

  std::uint8_t SendSequence() const;
  std::uint8_t ReceiveSequence() const;

private:
  std::uint8_t rawValue_;
};

} // namespace hdlc
} // namespace dlms
