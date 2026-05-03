# 01. HDLC C++ Codec API Requirements

## 1. General API Rules

The C++ API must follow these rules:

- C++11 only.
- No exceptions in public/runtime API paths.
- All operations return `HdlcStatus`.
- All pointer arguments must be checked.
- No global mutable state.
- Codec must be usable by both client and server code.
- The no-allocation API is the strict API.
- Convenience APIs may use STL containers but must be documented as convenience wrappers.

## 2. Status Codes

```cpp
enum class HdlcStatus
{
  Ok = 0,

  NeedMoreData,
  OutputBufferTooSmall,

  InvalidArgument,
  InvalidFlag,
  InvalidFrameFormat,
  InvalidFrameType,
  InvalidFrameLength,

  InvalidAddress,
  InvalidControlField,

  InvalidHeaderChecksum,
  InvalidFrameChecksum,

  FrameTooLarge,
  InformationFieldTooLarge,

  SegmentationError,
  SegmentationIncomplete,
  SegmentationOverflow,

  UnsupportedFrame,
  UnsupportedAddress,
  UnsupportedFeature,

  InternalError
};
```

## 3. Limits

```cpp
struct HdlcCodecLimits
{
  std::size_t maximumFrameSize;
  std::size_t maximumInformationFieldSize;
  std::size_t maximumReassembledInformationSize;
};
```

Default values:

```text
maximumFrameSize = 2049
maximumReassembledInformationSize = 65535
```

`maximumInformationFieldSize` is derived per frame from address sizes:

```text
maximumInformationFieldSize =
  maximumFrameSize
- 2 bytes flags
- header_size
- 2 bytes HCS
- 2 bytes FCS
```

where:

```text
header_size =
  2 bytes frame format
+ destination address size
+ source address size
+ 1 byte control
```

## 4. Address Type

```cpp
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
```

## 5. Control Field Type

The minimum supported set:

- I-frame
- RR
- RNR
- REJ
- SREJ
- SNRM
- UA
- DISC
- DM
- FRMR
- UI

```cpp
enum class HdlcFrameKind
{
  Information,
  Supervisory,
  Unnumbered
};

enum class HdlcSupervisoryKind
{
  ReceiveReady,
  ReceiveNotReady,
  Reject,
  SelectiveReject
};

enum class HdlcUnnumberedKind
{
  Snrm,
  Ua,
  Disc,
  Dm,
  Frmr,
  Ui
};

class HdlcControl
{
public:
  HdlcControl();

  static HdlcStatus Decode(
    std::uint8_t value,
    HdlcControl& control);

  HdlcStatus Encode(std::uint8_t& value) const;

  HdlcFrameKind FrameKind() const;
  bool PollFinal() const;

  std::uint8_t SendSequence() const;
  std::uint8_t ReceiveSequence() const;

private:
  std::uint8_t rawValue_;
};
```

## 6. Frame View

```cpp
struct HdlcFrame
{
  bool segmented;

  HdlcAddress destination;
  HdlcAddress source;

  HdlcControl control;

  const std::uint8_t* informationData;
  std::size_t informationSize;
};
```

## 7. Owning Frame Buffer

```cpp
struct HdlcFrameBuffer
{
  bool segmented;

  HdlcAddress destination;
  HdlcAddress source;

  HdlcControl control;

  std::vector<std::uint8_t> information;
};
```

`HdlcFrameBuffer` is allowed for convenience and tests. The strict no-allocation API must not require it.

## 8. Encoder API

Strict API:

```cpp
HdlcStatus EncodeFrameToBuffer(
  const HdlcFrame& frame,
  const HdlcCodecLimits& limits,
  std::uint8_t* output,
  std::size_t outputSize,
  std::size_t& writtenSize);
```

Convenience API:

```cpp
HdlcStatus EncodeFrame(
  const HdlcFrame& frame,
  const HdlcCodecLimits& limits,
  std::vector<std::uint8_t>& output);
```

## 9. Decoder API

Strict API:

```cpp
HdlcStatus DecodeFrameFromBuffer(
  const std::uint8_t* input,
  std::size_t inputSize,
  const HdlcCodecLimits& limits,
  HdlcFrame& frame,
  std::uint8_t* informationBuffer,
  std::size_t informationBufferSize,
  std::size_t& informationSize);
```

Convenience API:

```cpp
HdlcStatus DecodeFrame(
  const std::uint8_t* input,
  std::size_t inputSize,
  const HdlcCodecLimits& limits,
  HdlcFrameBuffer& frame);
```

## 10. Stream Decoder Policy

```cpp
enum class HdlcNoisePolicy
{
  IgnoreUntilOpeningFlag,
  ReportError
};
```

```cpp
struct HdlcStreamDecoderOptions
{
  HdlcCodecLimits limits;
  HdlcNoisePolicy noisePolicy;
};
```

The stream decoder must use Format Field length to determine the frame body size. It must not terminate a frame on the first `0x7E` found after the opening flag.

## 11. Closing Flag

Closing flag is mandatory.

A frame without closing flag must return:

```text
NeedMoreData
```

or, if the frame is already complete by length but the next byte is not a flag:

```text
InvalidFlag
```
