#pragma once

#include "dlms/hdlc/hdlc_error.hpp"

#include <cstddef>
#include <cstdint>

namespace dlms {
namespace hdlc {

/**
 * @brief Encoded DLMS/COSEM HDLC address value.
 *
 * The object stores an address as a raw concatenation of 7-bit address groups
 * with HDLC extension bits stripped, plus the encoded address size in bytes.
 * Supported encoded sizes are 1, 2, and 4 bytes.
 */
class HdlcAddress
{
public:
  /**
   * @brief Construct an empty address object.
   *
   * The default value is not a useful wire address until populated by
   * `FromBytes`, `FromRaw`, or a DLMS helper.
   */
  HdlcAddress();

  /**
   * @brief Decode an HDLC address field from wire bytes.
   *
   * The decoder consumes bytes until it finds the extension bit set in the
   * final address byte. Only 1-, 2-, and 4-byte encoded addresses are accepted.
   *
   * @param data Address bytes at the current frame offset.
   * @param size Number of available bytes at `data`.
   * @param address Receives the decoded address on success.
   * @param consumedSize Receives the number of bytes consumed from `data`.
   * @return `Ok` on success, otherwise an address or argument status.
   */
  static HdlcStatus FromBytes(
    const std::uint8_t* data,
    std::size_t size,
    HdlcAddress& address,
    std::size_t& consumedSize);

  /**
   * @brief Create an address from a raw value and encoded size.
   *
   * @param rawValue Concatenated 7-bit address groups without extension bits.
   * @param encodedSize Encoded wire size in bytes; must be 1, 2, or 4.
   * @param address Receives the constructed address on success.
   * @return `Ok` or an address validation status.
   */
  static HdlcStatus FromRaw(
    std::uint32_t rawValue,
    std::size_t encodedSize,
    HdlcAddress& address);

  /**
   * @brief Encode this address to caller-provided wire storage.
   *
   * @param output Destination buffer for encoded address bytes.
   * @param outputSize Size of `output` in bytes.
   * @param writtenSize Receives the number of bytes written on success.
   * @return `Ok`, `OutputBufferTooSmall`, or an address/argument status.
   */
  HdlcStatus Encode(
    std::uint8_t* output,
    std::size_t outputSize,
    std::size_t& writtenSize) const;

  /**
   * @brief Return the raw address value without HDLC extension bits.
   * @return Concatenated 7-bit address groups.
   */
  std::uint32_t RawValue() const;
  /**
   * @brief Return the encoded wire size of this address.
   * @return Address size in bytes: 1, 2, or 4 for supported addresses.
   */
  std::size_t EncodedSize() const;

private:
  std::uint32_t rawValue_;
  std::size_t encodedSize_;
};

/**
 * @brief Helper factory for common DLMS/COSEM client and server addresses.
 */
class DlmsHdlcAddress
{
public:
  /**
   * @brief Build a one-byte DLMS client HDLC address.
   *
   * @param clientAddress DLMS client address value in the supported range.
   * @param output Receives the encoded address object.
   * @return `Ok` or `UnsupportedAddress` if the value cannot be represented.
   */
  static HdlcStatus MakeClientAddress(
    std::uint8_t clientAddress,
    HdlcAddress& output);

  /**
   * @brief Build a DLMS server HDLC address from logical and physical parts.
   *
   * Produces a logical-only one-byte address when the physical address is zero
   * and the logical address fits. Otherwise produces a two- or four-byte server
   * address according to DLMS/COSEM HDLC address sizing rules.
   *
   * @param logicalDeviceAddress Logical device address component.
   * @param physicalDeviceAddress Physical device address component.
   * @param output Receives the encoded address object.
   * @return `Ok` or `UnsupportedAddress` if the values cannot be represented.
   */
  static HdlcStatus MakeServerAddress(
    std::uint16_t logicalDeviceAddress,
    std::uint16_t physicalDeviceAddress,
    HdlcAddress& output);
};

} // namespace hdlc
} // namespace dlms
