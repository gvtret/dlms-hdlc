#pragma once

#include <cstddef>

namespace dlms {
namespace hdlc {

/**
 * @brief Runtime limits used by frame codec, stream decoder, and reassembler.
 *
 * A zero field means "use the codec default" for APIs that accept limits.
 * The HDLC session layer may update these values after SNRM/UA negotiation,
 * but the codec itself does not perform negotiation.
 */
struct HdlcCodecLimits
{
  /// Maximum complete HDLC frame size in bytes, including both flag bytes.
  std::size_t maximumFrameSize;
  /// Maximum Information field size accepted in a single HDLC frame.
  std::size_t maximumInformationFieldSize;
  /// Maximum accumulated Information size accepted by reassembly.
  std::size_t maximumReassembledInformationSize;
};

/**
 * @brief Return the codec default safety limits.
 *
 * Defaults are `maximumFrameSize = 2049`, `maximumInformationFieldSize = 0`
 * (derive per frame), and `maximumReassembledInformationSize = 65535`.
 *
 * @return Default limit structure suitable for normal DLMS/COSEM HDLC use.
 */
HdlcCodecLimits DefaultHdlcCodecLimits();

} // namespace hdlc
} // namespace dlms
