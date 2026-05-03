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
 * @brief Policy for bytes received before the next HDLC opening flag.
 */
enum class HdlcNoisePolicy
{
  /// Discard leading noise bytes until an opening flag is found.
  IgnoreUntilOpeningFlag,
  /// Return `InvalidFlag` when leading noise bytes are observed.
  ReportError
};

/**
 * @brief Options used to construct an HDLC stream decoder.
 */
struct HdlcStreamDecoderOptions
{
  /// Frame and Information limits used for length checks and frame decoding.
  HdlcCodecLimits limits;
  /// Leading-noise handling policy.
  HdlcNoisePolicy noisePolicy;
};

/**
 * @brief Incremental HDLC stream decoder.
 *
 * The decoder accepts arbitrary byte chunks and extracts complete HDLC frames
 * using the Frame Format length field. It does not terminate frames on payload
 * bytes equal to `0x7e`; the closing flag is checked only at the length-derived
 * frame boundary.
 */
class HdlcStreamDecoder
{
public:
  /**
   * @brief Construct a stream decoder with fixed options.
   * @param options Decoder limits and leading-noise policy.
   */
  explicit HdlcStreamDecoder(const HdlcStreamDecoderOptions& options);

  /**
   * @brief Push bytes into the decoder and receive all complete frames.
   *
   * On success, zero or more frames may be appended to `frames`. When no full
   * frame is available yet, the method returns `NeedMoreData`.
   *
   * @param data Incoming byte chunk. May be null only when `size == 0`.
   * @param size Number of bytes at `data`.
   * @param frames Receives decoded complete frames; cleared at method entry.
   * @return `Ok`, `NeedMoreData`, or a frame/noise/allocation status.
   */
  HdlcStatus Push(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<HdlcFrameBuffer>& frames);

  /**
   * @brief Discard buffered partial input and return to initial state.
   */
  void Reset();

private:
  HdlcStreamDecoderOptions options_;
  std::vector<std::uint8_t> buffer_;
};

} // namespace hdlc
} // namespace dlms
