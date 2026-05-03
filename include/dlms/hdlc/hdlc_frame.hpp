#pragma once

#include "dlms/hdlc/hdlc_address.hpp"
#include "dlms/hdlc/hdlc_control.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace hdlc {

/**
 * @brief Non-owning HDLC frame view used by encoder and strict decode APIs.
 *
 * The frame references caller-owned Information bytes. The caller must keep
 * `informationData` valid for the duration of the API call that consumes it.
 */
struct HdlcFrame
{
  /// HDLC Frame Format segmentation bit; true means more segments follow.
  bool segmented;

  /// Destination HDLC address.
  HdlcAddress destination;
  /// Source HDLC address.
  HdlcAddress source;

  /// Parsed HDLC control field.
  HdlcControl control;

  /// Pointer to opaque Information field bytes, or null when empty.
  const std::uint8_t* informationData;
  /// Number of bytes available at `informationData`.
  std::size_t informationSize;
};

/**
 * @brief Owning HDLC frame container used by stream decoder and reassembler.
 *
 * This type owns the Information field bytes and is convenient for APIs that
 * may produce frames from internal buffers. It still stores parsed address and
 * control objects in the same form as `HdlcFrame`.
 */
struct HdlcFrameBuffer
{
  /// HDLC Frame Format segmentation bit; true means more segments follow.
  bool segmented;

  /// Destination HDLC address.
  HdlcAddress destination;
  /// Source HDLC address.
  HdlcAddress source;

  /// Parsed HDLC control field.
  HdlcControl control;

  /// Owned opaque Information field bytes.
  std::vector<std::uint8_t> information;
};

} // namespace hdlc
} // namespace dlms
