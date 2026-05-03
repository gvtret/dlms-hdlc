#include "dlms/hdlc/hdlc_crc.hpp"

#include <cstdint>

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::CalculateHdlcCrc;
using dlms::hdlc::HdlcStatus;
using dlms::hdlc::ValidateHdlcCrc;

std::uint16_t ReferenceBitwiseCrc(const std::uint8_t* data, std::size_t size)
{
  std::uint16_t crc = 0xffffu;

  for (std::size_t index = 0; index < size; ++index) {
    crc ^= data[index];
    for (int bit = 0; bit < 8; ++bit) {
      if ((crc & 0x0001u) != 0u) {
        crc = static_cast<std::uint16_t>((crc >> 1) ^ 0x8408u);
      } else {
        crc = static_cast<std::uint16_t>(crc >> 1);
      }
    }
  }

  return static_cast<std::uint16_t>(crc ^ 0xffffu);
}

TEST(HdlcCrc, CalculateEmpty)
{
  EXPECT_EQ(0x0000u, CalculateHdlcCrc(0, 0));
}

TEST(HdlcCrc, CalculateCanonicalCheckValue)
{
  const std::uint8_t data[] = {
    '1', '2', '3', '4', '5', '6', '7', '8', '9'
  };

  EXPECT_EQ(0x906eu, CalculateHdlcCrc(data, sizeof(data)));
}

TEST(HdlcCrc, CalculateKnownSnrm)
{
  const std::uint8_t frameWithoutFlagsAndFcs[] = {
    0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93
  };

  EXPECT_EQ(0x43e4u,
            CalculateHdlcCrc(frameWithoutFlagsAndFcs,
                             sizeof(frameWithoutFlagsAndFcs)));
}

TEST(HdlcCrc, CalculateKnownUaHeaderCheckSequence)
{
  const std::uint8_t headerWithoutHcs[] = {
    0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73
  };

  EXPECT_EQ(0x96b4u, CalculateHdlcCrc(headerWithoutHcs,
                                      sizeof(headerWithoutHcs)));
}

TEST(HdlcCrc, CalculateKnownUaFrameCheckSequence)
{
  const std::uint8_t frameWithoutFlagsAndFcs[] = {
    0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73, 0xb4, 0x96,
    0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06, 0x01,
    0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01, 0x08,
    0x04, 0x00, 0x00, 0x00, 0x01
  };

  EXPECT_EQ(0x755fu,
            CalculateHdlcCrc(frameWithoutFlagsAndFcs,
                             sizeof(frameWithoutFlagsAndFcs)));
}

TEST(HdlcCrc, ValidateValid)
{
  const std::uint8_t data[] = {0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93};

  EXPECT_EQ(HdlcStatus::Ok, ValidateHdlcCrc(data, sizeof(data), 0x43e4u));
}

TEST(HdlcCrc, ValidateInvalid)
{
  const std::uint8_t data[] = {0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93};

  EXPECT_EQ(HdlcStatus::InvalidFrameChecksum,
            ValidateHdlcCrc(data, sizeof(data), 0x43e5u));
}

TEST(HdlcCrc, ValidateRejectsNullDataWithNonZeroSize)
{
  EXPECT_EQ(HdlcStatus::InvalidArgument, ValidateHdlcCrc(0, 1, 0x0000u));
}

TEST(HdlcCrc, MatchesBitwiseReference)
{
  const std::uint8_t data[] = {
    0xa0, 0x1a, 0x02, 0x23, 0xc9, 0x32, 0xaf, 0x55,
    0xe6, 0xe6, 0x00, 0xc0, 0x01, 0x40, 0x00, 0x08,
    0x00, 0x00, 0x01, 0x00, 0x00, 0xff, 0x02, 0x00
  };

  EXPECT_EQ(ReferenceBitwiseCrc(data, sizeof(data)),
            CalculateHdlcCrc(data, sizeof(data)));
}

} // namespace
