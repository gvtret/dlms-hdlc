#include "dlms/hdlc/hdlc_address.hpp"

namespace dlms {
namespace hdlc {

namespace {

bool IsSupportedEncodedSize(std::size_t encodedSize)
{
  return encodedSize == 1u || encodedSize == 2u || encodedSize == 4u;
}

std::uint32_t MaximumRawValue(std::size_t encodedSize)
{
  return (std::uint32_t(1u) << (encodedSize * 7u)) - 1u;
}

} // namespace

HdlcAddress::HdlcAddress()
  : rawValue_(0u),
    encodedSize_(1u)
{
}

HdlcStatus HdlcAddress::FromBytes(
  const std::uint8_t* data,
  std::size_t size,
  HdlcAddress& address,
  std::size_t& consumedSize)
{
  consumedSize = 0u;

  if (data == 0) {
    return HdlcStatus::InvalidArgument;
  }

  std::uint32_t rawValue = 0u;
  for (std::size_t index = 0u; index < size && index < 4u; ++index) {
    rawValue = static_cast<std::uint32_t>((rawValue << 7) |
                                          ((data[index] >> 1) & 0x7fu));
    consumedSize = index + 1u;

    if ((data[index] & 0x01u) != 0u) {
      if (!IsSupportedEncodedSize(consumedSize)) {
        consumedSize = 0u;
        return HdlcStatus::UnsupportedAddress;
      }

      address.rawValue_ = rawValue;
      address.encodedSize_ = consumedSize;
      return HdlcStatus::Ok;
    }
  }

  consumedSize = 0u;
  if (size < 4u) {
    return HdlcStatus::NeedMoreData;
  }

  return HdlcStatus::InvalidAddress;
}

HdlcStatus HdlcAddress::FromRaw(
  std::uint32_t rawValue,
  std::size_t encodedSize,
  HdlcAddress& address)
{
  if (!IsSupportedEncodedSize(encodedSize)) {
    return HdlcStatus::UnsupportedAddress;
  }

  if (rawValue > MaximumRawValue(encodedSize)) {
    return HdlcStatus::InvalidAddress;
  }

  address.rawValue_ = rawValue;
  address.encodedSize_ = encodedSize;
  return HdlcStatus::Ok;
}

HdlcStatus HdlcAddress::Encode(
  std::uint8_t* output,
  std::size_t outputSize,
  std::size_t& writtenSize) const
{
  writtenSize = 0u;

  if (output == 0) {
    return HdlcStatus::InvalidArgument;
  }

  if (outputSize < encodedSize_) {
    return HdlcStatus::OutputBufferTooSmall;
  }

  for (std::size_t index = 0u; index < encodedSize_; ++index) {
    const std::size_t shift = (encodedSize_ - index - 1u) * 7u;
    std::uint8_t value = static_cast<std::uint8_t>((rawValue_ >> shift) &
                                                   0x7fu);
    value = static_cast<std::uint8_t>(value << 1);
    if (index + 1u == encodedSize_) {
      value = static_cast<std::uint8_t>(value | 0x01u);
    }
    output[index] = value;
  }

  writtenSize = encodedSize_;
  return HdlcStatus::Ok;
}

std::uint32_t HdlcAddress::RawValue() const
{
  return rawValue_;
}

std::size_t HdlcAddress::EncodedSize() const
{
  return encodedSize_;
}

HdlcStatus DlmsHdlcAddress::MakeClientAddress(
  std::uint8_t clientAddress,
  HdlcAddress& output)
{
  if (clientAddress > 0x7fu) {
    return HdlcStatus::InvalidAddress;
  }

  return HdlcAddress::FromRaw(clientAddress, 1u, output);
}

HdlcStatus DlmsHdlcAddress::MakeServerAddress(
  std::uint16_t logicalDeviceAddress,
  std::uint16_t physicalDeviceAddress,
  HdlcAddress& output)
{
  if (logicalDeviceAddress > 0x3fffu ||
      physicalDeviceAddress > 0x3fffu) {
    return HdlcStatus::InvalidAddress;
  }

  if (physicalDeviceAddress == 0u && logicalDeviceAddress <= 0x7fu) {
    return HdlcAddress::FromRaw(logicalDeviceAddress, 1u, output);
  }

  if (logicalDeviceAddress <= 0x7fu && physicalDeviceAddress <= 0x7fu) {
    const std::uint32_t rawValue =
      (std::uint32_t(logicalDeviceAddress) << 7) | physicalDeviceAddress;
    return HdlcAddress::FromRaw(rawValue, 2u, output);
  }

  const std::uint32_t rawValue =
    (std::uint32_t(logicalDeviceAddress) << 14) | physicalDeviceAddress;
  return HdlcAddress::FromRaw(rawValue, 4u, output);
}

} // namespace hdlc
} // namespace dlms
