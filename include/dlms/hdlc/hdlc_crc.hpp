#pragma once

#include "dlms/hdlc/hdlc_error.hpp"

#include <cstddef>
#include <cstdint>

namespace dlms {
namespace hdlc {

/**
 * @brief Calculate the DLMS/COSEM HDLC 16-bit FCS value.
 *
 * Uses the reflected HDLC FCS-16 algorithm with polynomial `0x8408`, initial
 * value `0xffff`, and final xor `0xffff`. The returned integer is the numeric
 * CRC value; HDLC frames place it on the wire low byte first.
 *
 * @param data Bytes covered by the CRC. May be null only when `size == 0`.
 * @param size Number of bytes covered by the CRC.
 * @return Calculated 16-bit HDLC CRC value.
 */
std::uint16_t CalculateHdlcCrc(
  const std::uint8_t* data,
  std::size_t size);

/**
 * @brief Validate bytes against an expected HDLC CRC value.
 *
 * @param data Bytes covered by the CRC. May be null only when `size == 0`.
 * @param size Number of bytes covered by the CRC.
 * @param expected Expected numeric CRC value.
 * @return `Ok` when the CRC matches, otherwise `InvalidFrameChecksum` or
 * `InvalidArgument`.
 */
HdlcStatus ValidateHdlcCrc(
  const std::uint8_t* data,
  std::size_t size,
  std::uint16_t expected);

} // namespace hdlc
} // namespace dlms
