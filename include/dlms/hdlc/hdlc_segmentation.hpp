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
 * @brief Options used by encoder-side HDLC segmentation.
 */
struct HdlcSegmentationOptions
{
  /// Limits used to choose per-frame Information chunk size.
  HdlcCodecLimits limits;
};

/**
 * @brief Splits an Information payload into HDLC frame-sized chunks.
 *
 * The segmenter copies metadata from a base frame and produces one or more
 * `HdlcFrameBuffer` values. Non-final chunks have `segmented == true`; the
 * final chunk has `segmented == false`.
 */
class HdlcSegmenter
{
public:
  /**
   * @brief Construct a segmenter with fixed limits.
   * @param options Segmentation options and limits.
   */
  explicit HdlcSegmenter(const HdlcSegmentationOptions& options);

  /**
   * @brief Segment an opaque Information payload.
   *
   * @param baseFrame Frame metadata copied to every output segment.
   * @param information Payload bytes to split; may be null only when size is 0.
   * @param informationSize Number of payload bytes.
   * @param outputFrames Receives generated frame buffers; cleared at entry.
   * @return `Ok` or an argument/allocation status.
   */
  HdlcStatus SegmentInformation(
    const HdlcFrame& baseFrame,
    const std::uint8_t* information,
    std::size_t informationSize,
    std::vector<HdlcFrameBuffer>& outputFrames);

private:
  HdlcSegmentationOptions options_;
};

/**
 * @brief Reassembles decoded HDLC segmented frames into complete payloads.
 *
 * The reassembler validates source address, destination address, compatible
 * frame kind, and accumulated Information size. It deliberately does not own
 * session-layer sequence validation, retransmission, or timeout behavior.
 */
class HdlcReassembler
{
public:
  /**
   * @brief Construct a reassembler with a maximum accumulated payload limit.
   * @param limits Limits used to enforce reassembled Information size.
   */
  explicit HdlcReassembler(const HdlcCodecLimits& limits);

  /**
   * @brief Push one decoded frame into the reassembly state machine.
   *
   * A non-segmented frame with no pending sequence completes immediately. A
   * segmented frame starts or continues a sequence and returns
   * `SegmentationIncomplete` until the final non-segmented frame arrives.
   *
   * @param frame Decoded frame to process.
   * @param completedFrame Receives a completed frame when available.
   * @param hasCompletedFrame Set to true only when `completedFrame` is valid.
   * @return `Ok`, `SegmentationIncomplete`, or a segmentation/limit status.
   */
  HdlcStatus PushFrame(
    const HdlcFrameBuffer& frame,
    HdlcFrameBuffer& completedFrame,
    bool& hasCompletedFrame);

  /**
   * @brief Clear any pending segmented sequence.
   */
  void Reset();

private:
  HdlcCodecLimits limits_;
  bool hasPending_;
  HdlcFrameBuffer pendingFrame_;
};

} // namespace hdlc
} // namespace dlms
