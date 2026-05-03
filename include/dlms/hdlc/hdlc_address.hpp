#pragma once

#include "dlms/hdlc/hdlc_error.hpp"

#include <cstddef>
#include <cstdint>

namespace dlms {
namespace hdlc {

class HdlcAddress
{
public:
  HdlcAddress();

  static HdlcStatus FromBytes(
    const std::uint8_t* data,
    std::size_t size,
    HdlcAddress& address,
    std::size_t& consumedSize);

  static HdlcStatus FromRaw(
    std::uint32_t rawValue,
    std::size_t encodedSize,
    HdlcAddress& address);

  HdlcStatus Encode(
    std::uint8_t* output,
    std::size_t outputSize,
    std::size_t& writtenSize) const;

  std::uint32_t RawValue() const;
  std::size_t EncodedSize() const;

private:
  std::uint32_t rawValue_;
  std::size_t encodedSize_;
};

class DlmsHdlcAddress
{
public:
  static HdlcStatus MakeClientAddress(
    std::uint8_t clientAddress,
    HdlcAddress& output);

  static HdlcStatus MakeServerAddress(
    std::uint16_t logicalDeviceAddress,
    std::uint16_t physicalDeviceAddress,
    HdlcAddress& output);
};

} // namespace hdlc
} // namespace dlms
