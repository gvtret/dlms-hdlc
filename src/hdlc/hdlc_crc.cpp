#include "dlms/hdlc/hdlc_crc.hpp"

namespace dlms {
namespace hdlc {

namespace {

const std::uint16_t kInitialFcs = 0xffffu;
const std::uint16_t kPolynomial = 0x8408u;

std::uint16_t UpdateCrc(std::uint16_t crc, std::uint8_t value)
{
  crc ^= value;
  for (int bit = 0; bit < 8; ++bit) {
    if ((crc & 0x0001u) != 0u) {
      crc = static_cast<std::uint16_t>((crc >> 1) ^ kPolynomial);
    } else {
      crc = static_cast<std::uint16_t>(crc >> 1);
    }
  }
  return crc;
}

} // namespace

std::uint16_t CalculateHdlcCrc(
  const std::uint8_t* data,
  std::size_t size)
{
  if (data == 0 && size != 0u) {
    return 0u;
  }

  std::uint16_t crc = kInitialFcs;
  for (std::size_t index = 0; index < size; ++index) {
    crc = UpdateCrc(crc, data[index]);
  }

  return static_cast<std::uint16_t>(crc ^ 0xffffu);
}

HdlcStatus ValidateHdlcCrc(
  const std::uint8_t* data,
  std::size_t size,
  std::uint16_t expected)
{
  if (data == 0 && size != 0u) {
    return HdlcStatus::InvalidArgument;
  }

  if (CalculateHdlcCrc(data, size) != expected) {
    return HdlcStatus::InvalidFrameChecksum;
  }

  return HdlcStatus::Ok;
}

} // namespace hdlc
} // namespace dlms
