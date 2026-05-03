#pragma once

#include <cstddef>

namespace dlms {
namespace hdlc {

struct HdlcCodecLimits
{
  std::size_t maximumFrameSize;
  std::size_t maximumInformationFieldSize;
  std::size_t maximumReassembledInformationSize;
};

HdlcCodecLimits DefaultHdlcCodecLimits();

} // namespace hdlc
} // namespace dlms
