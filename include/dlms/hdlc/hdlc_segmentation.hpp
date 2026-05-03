#pragma once

#include "dlms/hdlc/hdlc_error.hpp"
#include "dlms/hdlc/hdlc_frame.hpp"
#include "dlms/hdlc/hdlc_types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace hdlc {

struct HdlcSegmentationOptions
{
  HdlcCodecLimits limits;
};

class HdlcSegmenter
{
public:
  explicit HdlcSegmenter(const HdlcSegmentationOptions& options);

  HdlcStatus SegmentInformation(
    const HdlcFrame& baseFrame,
    const std::uint8_t* information,
    std::size_t informationSize,
    std::vector<HdlcFrameBuffer>& outputFrames);

private:
  HdlcSegmentationOptions options_;
};

class HdlcReassembler
{
public:
  explicit HdlcReassembler(const HdlcCodecLimits& limits);

  HdlcStatus PushFrame(
    const HdlcFrameBuffer& frame,
    HdlcFrameBuffer& completedFrame,
    bool& hasCompletedFrame);

  void Reset();

private:
  HdlcCodecLimits limits_;
  bool hasPending_;
  HdlcFrameBuffer pendingFrame_;
};

} // namespace hdlc
} // namespace dlms
