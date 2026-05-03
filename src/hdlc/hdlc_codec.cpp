#include "dlms/hdlc/hdlc_codec.hpp"

#include "dlms/hdlc/hdlc_crc.hpp"

#include <algorithm>
#include <new>

namespace dlms {
namespace hdlc {

namespace {

const std::uint8_t kHdlcFlag = 0x7eu;
const std::uint16_t kFrameFormatType3 = 0xa000u;
const std::uint16_t kSegmentationBit = 0x0800u;
const std::size_t kMaximumFormatFieldLength = 2047u;

std::size_t EffectiveMaximumFrameSize(const HdlcCodecLimits& limits)
{
  return limits.maximumFrameSize == 0u ? 2049u : limits.maximumFrameSize;
}

std::size_t EffectiveMaximumInformationFieldSize(
  const HdlcFrame& frame,
  const HdlcCodecLimits& limits)
{
  if (limits.maximumInformationFieldSize != 0u) {
    return limits.maximumInformationFieldSize;
  }

  const std::size_t headerSize =
    2u + frame.destination.EncodedSize() + frame.source.EncodedSize() + 1u;
  const std::size_t maximumFrameSize = EffectiveMaximumFrameSize(limits);

  if (maximumFrameSize < 2u + headerSize + 2u + 2u) {
    return 0u;
  }

  return maximumFrameSize - 2u - headerSize - 2u - 2u;
}

void WriteUint16LowByteFirst(std::uint16_t value, std::uint8_t* output)
{
  output[0] = static_cast<std::uint8_t>(value & 0xffu);
  output[1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
}

void WriteUint16HighByteFirst(std::uint16_t value, std::uint8_t* output)
{
  output[0] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
  output[1] = static_cast<std::uint8_t>(value & 0xffu);
}

std::uint16_t ReadUint16LowByteFirst(const std::uint8_t* input)
{
  return static_cast<std::uint16_t>(input[0] |
                                    (std::uint16_t(input[1]) << 8));
}

std::uint16_t ReadUint16HighByteFirst(const std::uint8_t* input)
{
  return static_cast<std::uint16_t>((std::uint16_t(input[0]) << 8) |
                                    input[1]);
}

} // namespace

HdlcCodecLimits DefaultHdlcCodecLimits()
{
  HdlcCodecLimits limits;
  limits.maximumFrameSize = 2049u;
  limits.maximumInformationFieldSize = 0u;
  limits.maximumReassembledInformationSize = 65535u;
  return limits;
}

HdlcStatus EncodeFrameToBuffer(
  const HdlcFrame& frame,
  const HdlcCodecLimits& limits,
  std::uint8_t* output,
  std::size_t outputSize,
  std::size_t& writtenSize)
{
  writtenSize = 0u;

  if (output == 0) {
    return HdlcStatus::InvalidArgument;
  }

  if (frame.informationData == 0 && frame.informationSize != 0u) {
    return HdlcStatus::InvalidArgument;
  }

  if (frame.informationSize >
      EffectiveMaximumInformationFieldSize(frame, limits)) {
    return HdlcStatus::InformationFieldTooLarge;
  }

  const bool hasInformation = frame.informationSize != 0u;
  const std::size_t headerSize =
    2u + frame.destination.EncodedSize() + frame.source.EncodedSize() + 1u;
  const std::size_t formatFieldLength =
    headerSize + (hasInformation ? 2u : 0u) + frame.informationSize + 2u;
  const std::size_t fullFrameSize = formatFieldLength + 2u;

  if (formatFieldLength > kMaximumFormatFieldLength) {
    return HdlcStatus::InvalidFrameLength;
  }

  if (fullFrameSize > EffectiveMaximumFrameSize(limits)) {
    return HdlcStatus::FrameTooLarge;
  }

  if (outputSize < fullFrameSize) {
    return HdlcStatus::OutputBufferTooSmall;
  }

  std::size_t offset = 0u;
  output[offset++] = kHdlcFlag;

  const std::uint16_t frameFormat =
    static_cast<std::uint16_t>(kFrameFormatType3 |
                               (frame.segmented ? kSegmentationBit : 0u) |
                               formatFieldLength);
  WriteUint16HighByteFirst(frameFormat, output + offset);
  offset += 2u;

  std::size_t addressSize = 0u;
  HdlcStatus status =
    frame.destination.Encode(output + offset, outputSize - offset, addressSize);
  if (status != HdlcStatus::Ok) {
    return status;
  }
  offset += addressSize;

  status = frame.source.Encode(output + offset, outputSize - offset, addressSize);
  if (status != HdlcStatus::Ok) {
    return status;
  }
  offset += addressSize;

  std::uint8_t control = 0u;
  status = frame.control.Encode(control);
  if (status != HdlcStatus::Ok) {
    return status;
  }
  output[offset++] = control;

  if (hasInformation) {
    const std::uint16_t hcs =
      CalculateHdlcCrc(output + 1u, headerSize);
    WriteUint16LowByteFirst(hcs, output + offset);
    offset += 2u;

    std::copy(frame.informationData,
              frame.informationData + frame.informationSize,
              output + offset);
    offset += frame.informationSize;
  }

  const std::uint16_t fcs =
    CalculateHdlcCrc(output + 1u, formatFieldLength - 2u);
  WriteUint16LowByteFirst(fcs, output + offset);
  offset += 2u;

  output[offset++] = kHdlcFlag;
  writtenSize = offset;
  return HdlcStatus::Ok;
}

HdlcStatus EncodeFrame(
  const HdlcFrame& frame,
  const HdlcCodecLimits& limits,
  std::vector<std::uint8_t>& output)
{
  const std::size_t maximumFrameSize = EffectiveMaximumFrameSize(limits);

  try {
    output.assign(maximumFrameSize, 0u);
  } catch (const std::bad_alloc&) {
    return HdlcStatus::InternalError;
  }

  std::size_t writtenSize = 0u;
  const HdlcStatus status =
    EncodeFrameToBuffer(frame,
                        limits,
                        output.empty() ? 0 : &output[0],
                        output.size(),
                        writtenSize);
  if (status != HdlcStatus::Ok) {
    output.clear();
    return status;
  }

  try {
    output.resize(writtenSize);
  } catch (const std::bad_alloc&) {
    output.clear();
    return HdlcStatus::InternalError;
  }

  return HdlcStatus::Ok;
}

HdlcStatus DecodeFrameFromBuffer(
  const std::uint8_t* input,
  std::size_t inputSize,
  const HdlcCodecLimits& limits,
  HdlcFrame& frame,
  std::uint8_t* informationBuffer,
  std::size_t informationBufferSize,
  std::size_t& informationSize)
{
  informationSize = 0u;
  frame.informationData = informationBuffer;
  frame.informationSize = 0u;

  if (input == 0) {
    return HdlcStatus::InvalidArgument;
  }

  if (informationBuffer == 0 && informationBufferSize != 0u) {
    return HdlcStatus::InvalidArgument;
  }

  if (inputSize < 4u) {
    return HdlcStatus::NeedMoreData;
  }

  if (input[0] != kHdlcFlag) {
    return HdlcStatus::InvalidFlag;
  }

  const std::uint16_t frameFormat = ReadUint16HighByteFirst(input + 1u);
  if ((frameFormat & 0xf000u) != kFrameFormatType3) {
    return HdlcStatus::InvalidFrameFormat;
  }

  const bool segmented = (frameFormat & kSegmentationBit) != 0u;
  const std::size_t formatFieldLength =
    static_cast<std::size_t>(frameFormat & 0x07ffu);
  const std::size_t fullFrameSize = formatFieldLength + 2u;

  if (formatFieldLength < 6u) {
    return HdlcStatus::InvalidFrameLength;
  }

  if (formatFieldLength > kMaximumFormatFieldLength) {
    return HdlcStatus::InvalidFrameLength;
  }

  if (fullFrameSize > EffectiveMaximumFrameSize(limits)) {
    return HdlcStatus::FrameTooLarge;
  }

  if (inputSize < fullFrameSize) {
    return HdlcStatus::NeedMoreData;
  }

  if (inputSize != fullFrameSize) {
    return HdlcStatus::InvalidFrameLength;
  }

  if (input[fullFrameSize - 1u] != kHdlcFlag) {
    return HdlcStatus::InvalidFlag;
  }

  std::size_t offset = 3u;
  std::size_t consumed = 0u;
  HdlcAddress destination;
  HdlcStatus status =
    HdlcAddress::FromBytes(input + offset,
                           fullFrameSize - 1u - offset,
                           destination,
                           consumed);
  if (status != HdlcStatus::Ok) {
    return status;
  }
  offset += consumed;

  HdlcAddress source;
  status = HdlcAddress::FromBytes(input + offset,
                                  fullFrameSize - 1u - offset,
                                  source,
                                  consumed);
  if (status != HdlcStatus::Ok) {
    return status;
  }
  offset += consumed;

  if (offset >= fullFrameSize - 3u) {
    return HdlcStatus::InvalidFrameLength;
  }

  HdlcControl control;
  status = HdlcControl::Decode(input[offset++], control);
  if (status != HdlcStatus::Ok) {
    return status;
  }

  const std::size_t headerSize = offset - 1u;
  const std::size_t bytesBeforeFcs = fullFrameSize - 3u - offset;
  if (bytesBeforeFcs == 1u) {
    return HdlcStatus::InvalidFrameLength;
  }

  const bool hasInformation = bytesBeforeFcs != 0u;
  if (hasInformation) {
    const std::uint16_t expectedHcs = ReadUint16LowByteFirst(input + offset);
    if (CalculateHdlcCrc(input + 1u, headerSize) != expectedHcs) {
      return HdlcStatus::InvalidHeaderChecksum;
    }
    offset += 2u;
  }

  const std::uint8_t* const fcsField = input + fullFrameSize - 3u;
  const std::uint16_t expectedFcs = ReadUint16LowByteFirst(fcsField);
  if (CalculateHdlcCrc(input + 1u, formatFieldLength - 2u) != expectedFcs) {
    return HdlcStatus::InvalidFrameChecksum;
  }

  const std::size_t decodedInformationSize =
    hasInformation ? (fullFrameSize - 3u - offset) : 0u;
  if (decodedInformationSize > informationBufferSize) {
    return HdlcStatus::OutputBufferTooSmall;
  }

  HdlcFrame decoded;
  decoded.segmented = segmented;
  decoded.destination = destination;
  decoded.source = source;
  decoded.control = control;
  decoded.informationData = informationBuffer;
  decoded.informationSize = decodedInformationSize;

  if (decodedInformationSize >
      EffectiveMaximumInformationFieldSize(decoded, limits)) {
    return HdlcStatus::InformationFieldTooLarge;
  }

  if (decodedInformationSize != 0u) {
    std::copy(input + offset,
              input + offset + decodedInformationSize,
              informationBuffer);
  }

  frame = decoded;
  informationSize = decodedInformationSize;
  return HdlcStatus::Ok;
}

HdlcStatus DecodeFrame(
  const std::uint8_t* input,
  std::size_t inputSize,
  const HdlcCodecLimits& limits,
  HdlcFrameBuffer& frame)
{
  const std::size_t maximumFrameSize = EffectiveMaximumFrameSize(limits);

  try {
    frame.information.assign(maximumFrameSize, 0u);
  } catch (const std::bad_alloc&) {
    return HdlcStatus::InternalError;
  }

  HdlcFrame decoded;
  std::size_t informationSize = 0u;
  const HdlcStatus status =
    DecodeFrameFromBuffer(input,
                          inputSize,
                          limits,
                          decoded,
                          frame.information.empty() ? 0 : &frame.information[0],
                          frame.information.size(),
                          informationSize);
  if (status != HdlcStatus::Ok) {
    frame.information.clear();
    return status;
  }

  frame.segmented = decoded.segmented;
  frame.destination = decoded.destination;
  frame.source = decoded.source;
  frame.control = decoded.control;

  try {
    frame.information.resize(informationSize);
  } catch (const std::bad_alloc&) {
    frame.information.clear();
    return HdlcStatus::InternalError;
  }

  return HdlcStatus::Ok;
}

} // namespace hdlc
} // namespace dlms
