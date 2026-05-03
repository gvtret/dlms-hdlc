#pragma once

#include "dlms/hdlc/hdlc_error.hpp"
#include "dlms/hdlc/hdlc_frame.hpp"
#include "dlms/hdlc/hdlc_types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace hdlc {

HdlcStatus EncodeFrameToBuffer(
  const HdlcFrame& frame,
  const HdlcCodecLimits& limits,
  std::uint8_t* output,
  std::size_t outputSize,
  std::size_t& writtenSize);

HdlcStatus EncodeFrame(
  const HdlcFrame& frame,
  const HdlcCodecLimits& limits,
  std::vector<std::uint8_t>& output);

HdlcStatus DecodeFrameFromBuffer(
  const std::uint8_t* input,
  std::size_t inputSize,
  const HdlcCodecLimits& limits,
  HdlcFrame& frame,
  std::uint8_t* informationBuffer,
  std::size_t informationBufferSize,
  std::size_t& informationSize);

HdlcStatus DecodeFrame(
  const std::uint8_t* input,
  std::size_t inputSize,
  const HdlcCodecLimits& limits,
  HdlcFrameBuffer& frame);

} // namespace hdlc
} // namespace dlms
