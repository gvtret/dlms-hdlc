#pragma once

namespace dlms {
namespace hdlc {

/**
 * @brief Status code returned by all public HDLC codec APIs.
 *
 * The library uses status codes instead of exceptions in public runtime paths.
 * Numeric values are part of the stable API contract and are mirrored by the
 * C ABI status enum in `hdlc_c_api.h`.
 */
enum class HdlcStatus
{
  /// Operation completed successfully.
  Ok = 0,

  /// More input bytes are required before a complete frame can be decoded.
  NeedMoreData,
  /// Caller-provided output or information buffer is too small.
  OutputBufferTooSmall,

  /// A pointer, size, option, or argument combination is invalid.
  InvalidArgument,
  /// Opening or closing HDLC flag is missing or invalid.
  InvalidFlag,
  /// Frame Format field does not contain supported HDLC Type 3 format.
  InvalidFrameFormat,
  /// Control field encodes a syntactically unsupported frame type.
  InvalidFrameType,
  /// Frame Format length is inconsistent, too small, or otherwise invalid.
  InvalidFrameLength,

  /// Destination or source HDLC address field is malformed.
  InvalidAddress,
  /// Control field is malformed or unsupported by the codec.
  InvalidControlField,

  /// Header Check Sequence verification failed for an information frame.
  InvalidHeaderChecksum,
  /// Frame Check Sequence verification failed.
  InvalidFrameChecksum,

  /// Frame exceeds configured or format-derived maximum frame size.
  FrameTooLarge,
  /// Information field exceeds the configured per-frame information limit.
  InformationFieldTooLarge,

  /// Segmented-frame sequence is inconsistent or cannot be continued.
  SegmentationError,
  /// A segmented sequence is valid so far but more frames are required.
  SegmentationIncomplete,
  /// Reassembled information would exceed configured reassembly limit.
  SegmentationOverflow,

  /// Frame is valid HDLC but outside the supported codec feature set.
  UnsupportedFrame,
  /// Address is valid HDLC but outside the supported address model.
  UnsupportedAddress,
  /// Requested feature is intentionally outside this codec layer.
  UnsupportedFeature,

  /// Unexpected internal failure, usually allocation failure in convenience APIs.
  InternalError
};

} // namespace hdlc
} // namespace dlms
