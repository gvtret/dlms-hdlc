#include "dlms/hdlc/hdlc_segmentation.hpp"

#include <algorithm>
#include <new>

namespace dlms {
namespace hdlc {

namespace {

std::size_t EffectiveMaximumInformationFieldSize(
  const HdlcCodecLimits& limits)
{
  return limits.maximumInformationFieldSize == 0u
    ? 2030u
    : limits.maximumInformationFieldSize;
}

std::size_t EffectiveMaximumReassembledInformationSize(
  const HdlcCodecLimits& limits)
{
  return limits.maximumReassembledInformationSize == 0u
    ? 65535u
    : limits.maximumReassembledInformationSize;
}

bool SameAddress(const HdlcAddress& left, const HdlcAddress& right)
{
  return left.RawValue() == right.RawValue() &&
         left.EncodedSize() == right.EncodedSize();
}

bool CompatibleFrameType(const HdlcControl& left, const HdlcControl& right)
{
  return left.FrameKind() == right.FrameKind();
}

HdlcFrameBuffer MakeFrameBuffer(
  const HdlcFrame& baseFrame,
  bool segmented,
  const std::uint8_t* information,
  std::size_t informationSize)
{
  HdlcFrameBuffer frame;
  frame.segmented = segmented;
  frame.destination = baseFrame.destination;
  frame.source = baseFrame.source;
  frame.control = baseFrame.control;
  if (informationSize != 0u) {
    frame.information.assign(information, information + informationSize);
  }
  return frame;
}

} // namespace

HdlcSegmenter::HdlcSegmenter(const HdlcSegmentationOptions& options)
  : options_(options)
{
}

HdlcStatus HdlcSegmenter::SegmentInformation(
  const HdlcFrame& baseFrame,
  const std::uint8_t* information,
  std::size_t informationSize,
  std::vector<HdlcFrameBuffer>& outputFrames)
{
  outputFrames.clear();

  if (information == 0 && informationSize != 0u) {
    return HdlcStatus::InvalidArgument;
  }

  const std::size_t maximumInformationFieldSize =
    EffectiveMaximumInformationFieldSize(options_.limits);
  if (maximumInformationFieldSize == 0u) {
    return HdlcStatus::InvalidArgument;
  }

  try {
    if (informationSize <= maximumInformationFieldSize) {
      outputFrames.push_back(MakeFrameBuffer(baseFrame,
                                             false,
                                             information,
                                             informationSize));
      return HdlcStatus::Ok;
    }

    std::size_t offset = 0u;
    while (offset < informationSize) {
      const std::size_t remaining = informationSize - offset;
      const std::size_t chunkSize =
        std::min(maximumInformationFieldSize, remaining);
      const bool hasMore = offset + chunkSize < informationSize;
      outputFrames.push_back(MakeFrameBuffer(baseFrame,
                                             hasMore,
                                             information + offset,
                                             chunkSize));
      offset += chunkSize;
    }
  } catch (const std::bad_alloc&) {
    outputFrames.clear();
    return HdlcStatus::InternalError;
  }

  return HdlcStatus::Ok;
}

HdlcReassembler::HdlcReassembler(const HdlcCodecLimits& limits)
  : limits_(limits),
    hasPending_(false),
    pendingFrame_()
{
}

HdlcStatus HdlcReassembler::PushFrame(
  const HdlcFrameBuffer& frame,
  HdlcFrameBuffer& completedFrame,
  bool& hasCompletedFrame)
{
  hasCompletedFrame = false;
  completedFrame.information.clear();

  const std::size_t maximumReassembledInformationSize =
    EffectiveMaximumReassembledInformationSize(limits_);

  if (!hasPending_) {
    if (frame.information.size() > maximumReassembledInformationSize) {
      return HdlcStatus::SegmentationOverflow;
    }

    if (!frame.segmented) {
      completedFrame = frame;
      hasCompletedFrame = true;
      return HdlcStatus::Ok;
    }

    pendingFrame_ = frame;
    hasPending_ = true;
    return HdlcStatus::SegmentationIncomplete;
  }

  if (!SameAddress(pendingFrame_.destination, frame.destination) ||
      !SameAddress(pendingFrame_.source, frame.source) ||
      !CompatibleFrameType(pendingFrame_.control, frame.control)) {
    Reset();
    return HdlcStatus::SegmentationError;
  }

  if (pendingFrame_.information.size() >
      maximumReassembledInformationSize - frame.information.size()) {
    Reset();
    return HdlcStatus::SegmentationOverflow;
  }

  try {
    pendingFrame_.information.insert(pendingFrame_.information.end(),
                                     frame.information.begin(),
                                     frame.information.end());
  } catch (const std::bad_alloc&) {
    Reset();
    return HdlcStatus::InternalError;
  }

  if (frame.segmented) {
    return HdlcStatus::SegmentationIncomplete;
  }

  completedFrame = pendingFrame_;
  completedFrame.segmented = false;
  hasCompletedFrame = true;
  Reset();
  return HdlcStatus::Ok;
}

void HdlcReassembler::Reset()
{
  hasPending_ = false;
  pendingFrame_ = HdlcFrameBuffer();
}

} // namespace hdlc
} // namespace dlms
