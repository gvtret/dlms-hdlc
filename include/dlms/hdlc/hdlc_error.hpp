#pragma once

namespace dlms {
namespace hdlc {

enum class HdlcStatus
{
  Ok = 0,

  NeedMoreData,
  OutputBufferTooSmall,

  InvalidArgument,
  InvalidFlag,
  InvalidFrameFormat,
  InvalidFrameType,
  InvalidFrameLength,

  InvalidAddress,
  InvalidControlField,

  InvalidHeaderChecksum,
  InvalidFrameChecksum,

  FrameTooLarge,
  InformationFieldTooLarge,

  SegmentationError,
  SegmentationIncomplete,
  SegmentationOverflow,

  UnsupportedFrame,
  UnsupportedAddress,
  UnsupportedFeature,

  InternalError
};

} // namespace hdlc
} // namespace dlms
