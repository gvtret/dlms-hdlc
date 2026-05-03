#include "dlms/hdlc/hdlc_stream_decoder.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using dlms::hdlc::DefaultHdlcCodecLimits;
using dlms::hdlc::HdlcFrameBuffer;
using dlms::hdlc::HdlcNoisePolicy;
using dlms::hdlc::HdlcStatus;
using dlms::hdlc::HdlcStreamDecoder;
using dlms::hdlc::HdlcStreamDecoderOptions;

HdlcStreamDecoderOptions Options(HdlcNoisePolicy policy)
{
  HdlcStreamDecoderOptions options;
  options.limits = DefaultHdlcCodecLimits();
  options.noisePolicy = policy;
  return options;
}

const std::uint8_t kSnrmFrame[] = {
  0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
};

const std::uint8_t kRrFrame[] = {
  0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x11, 0xfe, 0xe4, 0x7e
};

const std::uint8_t kIFrameWithPayloadFlag[] = {
  0x7e, 0xa8, 0x10, 0x02, 0x23, 0xc9, 0x32, 0x5f,
  0x38, 0xe6, 0xe6, 0x00, 0x01, 0x7e, 0x02, 0xfe,
  0x1a, 0x7e
};

TEST(HdlcStreamDecoder, PushFullFrame)
{
  HdlcStreamDecoder decoder(Options(HdlcNoisePolicy::IgnoreUntilOpeningFlag));
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::Ok, decoder.Push(kSnrmFrame, sizeof(kSnrmFrame), frames));
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(0x91u, frames[0].destination.RawValue());
}

TEST(HdlcStreamDecoder, PushByteByByte)
{
  HdlcStreamDecoder decoder(Options(HdlcNoisePolicy::IgnoreUntilOpeningFlag));
  std::vector<HdlcFrameBuffer> frames;

  for (std::size_t index = 0u; index + 1u < sizeof(kSnrmFrame); ++index) {
    EXPECT_EQ(HdlcStatus::NeedMoreData,
              decoder.Push(kSnrmFrame + index, 1u, frames));
    EXPECT_TRUE(frames.empty());
  }

  EXPECT_EQ(HdlcStatus::Ok,
            decoder.Push(kSnrmFrame + sizeof(kSnrmFrame) - 1u, 1u, frames));
  ASSERT_EQ(1u, frames.size());
}

TEST(HdlcStreamDecoder, PushMultipleFrames)
{
  const std::uint8_t input[] = {
    0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e,
    0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x11, 0xfe, 0xe4, 0x7e
  };
  HdlcStreamDecoder decoder(Options(HdlcNoisePolicy::IgnoreUntilOpeningFlag));
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::Ok, decoder.Push(input, sizeof(input), frames));
  ASSERT_EQ(2u, frames.size());
}

TEST(HdlcStreamDecoder, PushFrameWithPayloadFlagByte)
{
  HdlcStreamDecoder decoder(Options(HdlcNoisePolicy::IgnoreUntilOpeningFlag));
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::Ok,
            decoder.Push(kIFrameWithPayloadFlag,
                         sizeof(kIFrameWithPayloadFlag),
                         frames));
  ASSERT_EQ(1u, frames.size());
  ASSERT_EQ(6u, frames[0].information.size());
  EXPECT_EQ(0x7eu, frames[0].information[4]);
}

TEST(HdlcStreamDecoder, PushNoiseBeforeFlagIgnorePolicy)
{
  const std::uint8_t input[] = {
    0x00, 0xff, 0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
  };
  HdlcStreamDecoder decoder(Options(HdlcNoisePolicy::IgnoreUntilOpeningFlag));
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::Ok, decoder.Push(input, sizeof(input), frames));
  ASSERT_EQ(1u, frames.size());
}

TEST(HdlcStreamDecoder, PushNoiseBeforeFlagErrorPolicy)
{
  const std::uint8_t input[] = {0x00, 0xff};
  HdlcStreamDecoder decoder(Options(HdlcNoisePolicy::ReportError));
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::InvalidFlag,
            decoder.Push(input, sizeof(input), frames));
}

TEST(HdlcStreamDecoder, PushInvalidLength)
{
  const std::uint8_t input[] = {
    0x7e, 0xa0, 0x05, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
  };
  HdlcStreamDecoder decoder(Options(HdlcNoisePolicy::IgnoreUntilOpeningFlag));
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::InvalidFrameLength,
            decoder.Push(input, sizeof(input), frames));
}

TEST(HdlcStreamDecoder, PushFrameTooLarge)
{
  HdlcStreamDecoderOptions options = Options(HdlcNoisePolicy::IgnoreUntilOpeningFlag);
  options.limits.maximumFrameSize = 9u;
  HdlcStreamDecoder decoder(options);
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::FrameTooLarge,
            decoder.Push(kSnrmFrame, sizeof(kSnrmFrame), frames));
}

TEST(HdlcStreamDecoder, PushMissingClosingFlag)
{
  HdlcStreamDecoder decoder(Options(HdlcNoisePolicy::IgnoreUntilOpeningFlag));
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::NeedMoreData,
            decoder.Push(kSnrmFrame, sizeof(kSnrmFrame) - 1u, frames));
  EXPECT_TRUE(frames.empty());
}

TEST(HdlcStreamDecoder, PushWrongClosingFlag)
{
  const std::uint8_t input[] = {
    0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x00
  };
  HdlcStreamDecoder decoder(Options(HdlcNoisePolicy::IgnoreUntilOpeningFlag));
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::InvalidFlag, decoder.Push(input, sizeof(input), frames));
}

TEST(HdlcStreamDecoder, PushResetAfterError)
{
  const std::uint8_t invalid[] = {
    0x7e, 0xa0, 0x05, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e
  };
  HdlcStreamDecoder decoder(Options(HdlcNoisePolicy::IgnoreUntilOpeningFlag));
  std::vector<HdlcFrameBuffer> frames;

  EXPECT_EQ(HdlcStatus::InvalidFrameLength,
            decoder.Push(invalid, sizeof(invalid), frames));
  decoder.Reset();
  EXPECT_EQ(HdlcStatus::Ok, decoder.Push(kRrFrame, sizeof(kRrFrame), frames));
  ASSERT_EQ(1u, frames.size());
}

} // namespace
