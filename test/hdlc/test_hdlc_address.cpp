#include "dlms/hdlc/hdlc_address.hpp"

#include <cstdint>

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::DlmsHdlcAddress;
using dlms::hdlc::HdlcAddress;
using dlms::hdlc::HdlcStatus;

TEST(HdlcAddress, DecodeOneByte)
{
  const std::uint8_t data[] = {0x75};
  HdlcAddress address;
  std::size_t consumed = 0u;

  EXPECT_EQ(HdlcStatus::Ok,
            HdlcAddress::FromBytes(data, sizeof(data), address, consumed));
  EXPECT_EQ(1u, consumed);
  EXPECT_EQ(0x3au, address.RawValue());
  EXPECT_EQ(1u, address.EncodedSize());
}

TEST(HdlcAddress, DecodeTwoBytes)
{
  const std::uint8_t data[] = {0x00, 0x21};
  HdlcAddress address;
  std::size_t consumed = 0u;

  EXPECT_EQ(HdlcStatus::Ok,
            HdlcAddress::FromBytes(data, sizeof(data), address, consumed));
  EXPECT_EQ(2u, consumed);
  EXPECT_EQ(0x0010u, address.RawValue());
  EXPECT_EQ(2u, address.EncodedSize());
}

TEST(HdlcAddress, DecodeFourBytes)
{
  const std::uint8_t data[] = {0x48, 0x68, 0xfe, 0xff};
  HdlcAddress address;
  std::size_t consumed = 0u;

  EXPECT_EQ(HdlcStatus::Ok,
            HdlcAddress::FromBytes(data, sizeof(data), address, consumed));
  EXPECT_EQ(4u, consumed);
  EXPECT_EQ(0x048d3fffu, address.RawValue());
  EXPECT_EQ(4u, address.EncodedSize());
}

TEST(HdlcAddress, DecodeTruncatedAddress)
{
  const std::uint8_t data[] = {0x48, 0x68};
  HdlcAddress address;
  std::size_t consumed = 0u;

  EXPECT_EQ(HdlcStatus::NeedMoreData,
            HdlcAddress::FromBytes(data, sizeof(data), address, consumed));
  EXPECT_EQ(0u, consumed);
}

TEST(HdlcAddress, DecodeInvalidExtensionBit)
{
  const std::uint8_t data[] = {0x48, 0x68, 0xfe, 0xfe};
  HdlcAddress address;
  std::size_t consumed = 0u;

  EXPECT_EQ(HdlcStatus::InvalidAddress,
            HdlcAddress::FromBytes(data, sizeof(data), address, consumed));
  EXPECT_EQ(0u, consumed);
}

TEST(HdlcAddress, DecodeRejectsNullData)
{
  HdlcAddress address;
  std::size_t consumed = 0u;

  EXPECT_EQ(HdlcStatus::InvalidArgument,
            HdlcAddress::FromBytes(0, 1u, address, consumed));
}

TEST(HdlcAddress, EncodeOneByte)
{
  HdlcAddress address;
  std::uint8_t output[1] = {};
  std::size_t written = 0u;

  ASSERT_EQ(HdlcStatus::Ok, HdlcAddress::FromRaw(0x3au, 1u, address));
  EXPECT_EQ(HdlcStatus::Ok, address.Encode(output, sizeof(output), written));
  EXPECT_EQ(1u, written);
  EXPECT_EQ(0x75u, output[0]);
}

TEST(HdlcAddress, EncodeTwoBytes)
{
  HdlcAddress address;
  std::uint8_t output[2] = {};
  std::size_t written = 0u;

  ASSERT_EQ(HdlcStatus::Ok, HdlcAddress::FromRaw(0x0010u, 2u, address));
  EXPECT_EQ(HdlcStatus::Ok, address.Encode(output, sizeof(output), written));
  EXPECT_EQ(2u, written);
  EXPECT_EQ(0x00u, output[0]);
  EXPECT_EQ(0x21u, output[1]);
}

TEST(HdlcAddress, EncodeFourBytes)
{
  HdlcAddress address;
  std::uint8_t output[4] = {};
  std::size_t written = 0u;

  ASSERT_EQ(HdlcStatus::Ok, HdlcAddress::FromRaw(0x048d3fffu, 4u, address));
  EXPECT_EQ(HdlcStatus::Ok, address.Encode(output, sizeof(output), written));
  EXPECT_EQ(4u, written);
  EXPECT_EQ(0x48u, output[0]);
  EXPECT_EQ(0x68u, output[1]);
  EXPECT_EQ(0xfeu, output[2]);
  EXPECT_EQ(0xffu, output[3]);
}

TEST(HdlcAddress, EncodeReportsSmallOutputBuffer)
{
  HdlcAddress address;
  std::uint8_t output[1] = {};
  std::size_t written = 99u;

  ASSERT_EQ(HdlcStatus::Ok, HdlcAddress::FromRaw(0x0010u, 2u, address));
  EXPECT_EQ(HdlcStatus::OutputBufferTooSmall,
            address.Encode(output, sizeof(output), written));
  EXPECT_EQ(0u, written);
}

TEST(HdlcAddress, FromRawRejectsUnsupportedSize)
{
  HdlcAddress address;

  EXPECT_EQ(HdlcStatus::UnsupportedAddress,
            HdlcAddress::FromRaw(0x00u, 3u, address));
}

TEST(HdlcAddress, FromRawRejectsValueTooLargeForSize)
{
  HdlcAddress address;

  EXPECT_EQ(HdlcStatus::InvalidAddress,
            HdlcAddress::FromRaw(0x80u, 1u, address));
}

TEST(DlmsHdlcAddress, MakeClientAddressValid)
{
  HdlcAddress address;
  std::uint8_t output[1] = {};
  std::size_t written = 0u;

  ASSERT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeClientAddress(0x10u, address));
  EXPECT_EQ(HdlcStatus::Ok, address.Encode(output, sizeof(output), written));
  EXPECT_EQ(0x21u, output[0]);
}

TEST(DlmsHdlcAddress, MakeClientAddressRejectsExtendedRange)
{
  HdlcAddress address;

  EXPECT_EQ(HdlcStatus::InvalidAddress,
            DlmsHdlcAddress::MakeClientAddress(0x80u, address));
}

TEST(DlmsHdlcAddress, MakeServerAddressLogicalOnly)
{
  HdlcAddress address;
  std::uint8_t output[1] = {};
  std::size_t written = 0u;

  ASSERT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeServerAddress(0x01u, 0x00u, address));
  EXPECT_EQ(HdlcStatus::Ok, address.Encode(output, sizeof(output), written));
  EXPECT_EQ(0x03u, output[0]);
}

TEST(DlmsHdlcAddress, MakeServerAddressLogicalPhysical)
{
  HdlcAddress address;
  std::uint8_t output[2] = {};
  std::size_t written = 0u;

  ASSERT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeServerAddress(0x01u, 0x10u, address));
  EXPECT_EQ(HdlcStatus::Ok, address.Encode(output, sizeof(output), written));
  EXPECT_EQ(0x02u, output[0]);
  EXPECT_EQ(0x21u, output[1]);
}

TEST(DlmsHdlcAddress, MakeServerAddressFourByteLogicalPhysical)
{
  HdlcAddress address;
  std::uint8_t output[4] = {};
  std::size_t written = 0u;

  ASSERT_EQ(HdlcStatus::Ok,
            DlmsHdlcAddress::MakeServerAddress(0x1234u, 0x3fffu, address));
  EXPECT_EQ(HdlcStatus::Ok, address.Encode(output, sizeof(output), written));
  EXPECT_EQ(0x48u, output[0]);
  EXPECT_EQ(0x68u, output[1]);
  EXPECT_EQ(0xfeu, output[2]);
  EXPECT_EQ(0xffu, output[3]);
}

} // namespace
