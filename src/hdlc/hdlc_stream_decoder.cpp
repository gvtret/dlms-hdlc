#include "dlms/hdlc/hdlc_stream_decoder.hpp"

#include "dlms/hdlc/hdlc_codec.hpp"

#include <algorithm>
#include <new>

namespace dlms {
namespace hdlc {

namespace {

const std::uint8_t kHdlcFlag = 0x7eu;
const std::uint16_t kFrameFormatType3 = 0xa000u;
const std::uint16_t kFrameFormatTypeMask = 0xf000u;

std::size_t EffectiveMaximumFrameSize(const HdlcCodecLimits& limits)
{
  return limits.maximumFrameSize == 0u ? 2049u : limits.maximumFrameSize;
}

std::uint16_t ReadUint16HighByteFirst(const std::uint8_t* input)
{
  return static_cast<std::uint16_t>((std::uint16_t(input[0]) << 8) |
                                    input[1]);
}

} // namespace

HdlcStreamDecoder::HdlcStreamDecoder(const HdlcStreamDecoderOptions& options)
  : options_(options),
    buffer_()
{
}

HdlcStatus HdlcStreamDecoder::Push(
  const std::uint8_t* data,
  std::size_t size,
  std::vector<HdlcFrameBuffer>& frames)
{
  frames.clear();

  if (data == 0 && size != 0u) {
    return HdlcStatus::InvalidArgument;
  }

  try {
    buffer_.insert(buffer_.end(), data, data + size);
  } catch (const std::bad_alloc&) {
    Reset();
    return HdlcStatus::InternalError;
  }

  for (;;) {
    std::vector<std::uint8_t>::iterator flag =
      std::find(buffer_.begin(), buffer_.end(), kHdlcFlag);
    if (flag == buffer_.end()) {
      if (!buffer_.empty() &&
          options_.noisePolicy == HdlcNoisePolicy::ReportError) {
        Reset();
        return HdlcStatus::InvalidFlag;
      }

      buffer_.clear();
      return frames.empty() ? HdlcStatus::NeedMoreData : HdlcStatus::Ok;
    }

    if (flag != buffer_.begin()) {
      if (options_.noisePolicy == HdlcNoisePolicy::ReportError) {
        Reset();
        return HdlcStatus::InvalidFlag;
      }

      buffer_.erase(buffer_.begin(), flag);
    }

    if (buffer_.size() < 3u) {
      return frames.empty() ? HdlcStatus::NeedMoreData : HdlcStatus::Ok;
    }

    const std::uint16_t frameFormat = ReadUint16HighByteFirst(&buffer_[1]);
    if ((frameFormat & kFrameFormatTypeMask) != kFrameFormatType3) {
      Reset();
      return HdlcStatus::InvalidFrameFormat;
    }

    const std::size_t formatFieldLength =
      static_cast<std::size_t>(frameFormat & 0x07ffu);
    const std::size_t fullFrameSize = formatFieldLength + 2u;

    if (formatFieldLength < 6u) {
      Reset();
      return HdlcStatus::InvalidFrameLength;
    }

    if (fullFrameSize > EffectiveMaximumFrameSize(options_.limits)) {
      Reset();
      return HdlcStatus::FrameTooLarge;
    }

    if (buffer_.size() < fullFrameSize) {
      return frames.empty() ? HdlcStatus::NeedMoreData : HdlcStatus::Ok;
    }

    HdlcFrameBuffer frame;
    const HdlcStatus status =
      DecodeFrame(&buffer_[0],
                  fullFrameSize,
                  options_.limits,
                  frame);
    if (status != HdlcStatus::Ok) {
      Reset();
      return status;
    }

    try {
      frames.push_back(frame);
    } catch (const std::bad_alloc&) {
      Reset();
      return HdlcStatus::InternalError;
    }

    buffer_.erase(buffer_.begin(), buffer_.begin() + fullFrameSize);
  }
}

void HdlcStreamDecoder::Reset()
{
  buffer_.clear();
}

} // namespace hdlc
} // namespace dlms
