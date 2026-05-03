#pragma once

#include "dlms/hdlc/hdlc_error.hpp"
#include "dlms/hdlc/hdlc_frame.hpp"
#include "dlms/hdlc/hdlc_types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace hdlc {

/**
 * @brief Encode a complete HDLC Type 3 frame into caller-provided storage.
 *
 * The encoder writes the opening flag, Frame Format field, destination and
 * source addresses, control field, optional HCS, opaque Information bytes, FCS,
 * and closing flag. It does not perform byte stuffing and does not allocate.
 *
 * @param frame Non-owning frame view to encode.
 * @param limits Runtime frame and information-size limits.
 * @param output Destination buffer for the complete encoded frame.
 * @param outputSize Size of `output` in bytes.
 * @param writtenSize Receives the number of bytes written on success.
 * @return `Ok` on success, or a validation/buffer status on failure.
 */
HdlcStatus EncodeFrameToBuffer(
  const HdlcFrame& frame,
  const HdlcCodecLimits& limits,
  std::uint8_t* output,
  std::size_t outputSize,
  std::size_t& writtenSize);

/**
 * @brief Encode a complete HDLC Type 3 frame into an owning byte vector.
 *
 * This convenience API may allocate internally. Allocation failures are caught
 * and returned as `InternalError`.
 *
 * @param frame Non-owning frame view to encode.
 * @param limits Runtime frame and information-size limits.
 * @param output Receives the encoded frame bytes on success; cleared on error.
 * @return `Ok` on success, or a validation/allocation status on failure.
 */
HdlcStatus EncodeFrame(
  const HdlcFrame& frame,
  const HdlcCodecLimits& limits,
  std::vector<std::uint8_t>& output);

/**
 * @brief Decode one complete HDLC Type 3 frame using caller-provided storage.
 *
 * The decoder validates opening and closing flags, frame format type, length,
 * addresses, control field, optional HCS, FCS, and configured limits. The
 * decoded Information field is copied into `informationBuffer`.
 *
 * @param input Complete encoded frame bytes, including opening and closing flags.
 * @param inputSize Number of bytes at `input`.
 * @param limits Runtime frame and information-size limits.
 * @param frame Receives a decoded non-owning frame view.
 * @param informationBuffer Caller-provided storage for decoded Information bytes.
 * @param informationBufferSize Size of `informationBuffer` in bytes.
 * @param informationSize Receives the decoded Information field size.
 * @return `Ok` on success, or a validation/buffer status on failure.
 */
HdlcStatus DecodeFrameFromBuffer(
  const std::uint8_t* input,
  std::size_t inputSize,
  const HdlcCodecLimits& limits,
  HdlcFrame& frame,
  std::uint8_t* informationBuffer,
  std::size_t informationBufferSize,
  std::size_t& informationSize);

/**
 * @brief Decode one complete HDLC Type 3 frame into an owning frame container.
 *
 * This convenience API may allocate for the owned Information field. Allocation
 * failures are caught and returned as `InternalError`.
 *
 * @param input Complete encoded frame bytes, including opening and closing flags.
 * @param inputSize Number of bytes at `input`.
 * @param limits Runtime frame and information-size limits.
 * @param frame Receives the decoded owning frame on success.
 * @return `Ok` on success, or a validation/allocation status on failure.
 */
HdlcStatus DecodeFrame(
  const std::uint8_t* input,
  std::size_t inputSize,
  const HdlcCodecLimits& limits,
  HdlcFrameBuffer& frame);

} // namespace hdlc
} // namespace dlms
