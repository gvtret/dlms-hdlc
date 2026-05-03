#pragma once

#include "dlms/hdlc/hdlc_address.hpp"
#include "dlms/hdlc/hdlc_control.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace hdlc {

struct HdlcFrame
{
  bool segmented;

  HdlcAddress destination;
  HdlcAddress source;

  HdlcControl control;

  const std::uint8_t* informationData;
  std::size_t informationSize;
};

struct HdlcFrameBuffer
{
  bool segmented;

  HdlcAddress destination;
  HdlcAddress source;

  HdlcControl control;

  std::vector<std::uint8_t> information;
};

} // namespace hdlc
} // namespace dlms
