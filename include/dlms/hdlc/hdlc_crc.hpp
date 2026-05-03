#pragma once

#include "dlms/hdlc/hdlc_error.hpp"

#include <cstddef>
#include <cstdint>

namespace dlms {
namespace hdlc {

std::uint16_t CalculateHdlcCrc(
  const std::uint8_t* data,
  std::size_t size);

HdlcStatus ValidateHdlcCrc(
  const std::uint8_t* data,
  std::size_t size,
  std::uint16_t expected);

} // namespace hdlc
} // namespace dlms
