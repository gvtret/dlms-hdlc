#pragma once

#include "dlms/hdlc/hdlc_error.hpp"
#include "dlms/hdlc/hdlc_frame.hpp"
#include "dlms/hdlc/hdlc_types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace hdlc {

enum class HdlcNoisePolicy
{
  IgnoreUntilOpeningFlag,
  ReportError
};

struct HdlcStreamDecoderOptions
{
  HdlcCodecLimits limits;
  HdlcNoisePolicy noisePolicy;
};

class HdlcStreamDecoder
{
public:
  explicit HdlcStreamDecoder(const HdlcStreamDecoderOptions& options);

  HdlcStatus Push(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<HdlcFrameBuffer>& frames);

  void Reset();

private:
  HdlcStreamDecoderOptions options_;
  std::vector<std::uint8_t> buffer_;
};

} // namespace hdlc
} // namespace dlms
