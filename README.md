# DLMS/COSEM HDLC Codec

Portable C++11 HDLC Frame Format Type 3 codec for DLMS/COSEM.

This repository contains the HDLC codec layer for a future DLMS/COSEM framework. The framework is planned to include HDLC, LLC, WRAPPER and APDU codecs, but this repository phase focuses on the HDLC codec foundation.

## Scope

Version 1 implements the **codec layer only**.

Included:

- HDLC Frame Format Type 3 encoding and decoding
- client and server frame support
- HDLC address encoding and decoding
- HDLC control field encoding and decoding
- HCS/FCS calculation and validation
- stream decoding by Format Field length
- full HDLC segmentation and reassembly
- status-code based error handling
- no exceptions in public/runtime API paths
- stable C ABI wrapper
- CMake 3.16+ build system
- GoogleTest-based test suite

Not included in v1:

- HDLC session state machine
- SNRM/UA negotiation logic
- timeout handling
- retransmission
- transport layer
- LLC codec
- WRAPPER codec
- APDU codec
- APDU block transfer
- security and ciphering

## Target Architecture

Expected future HDLC-based DLMS/COSEM stack:

```text
+-----------------------------+
| APDU codec                  |
+-----------------------------+
| LLC codec                   |
+-----------------------------+
| HDLC session layer          |
+-----------------------------+
| HDLC codec                  |
+-----------------------------+
| Transport: UART/TCP/etc.    |
+-----------------------------+
```

Expected future WRAPPER-based stack:

```text
+-----------------------------+
| APDU codec                  |
+-----------------------------+
| WRAPPER codec               |
+-----------------------------+
| Transport: TCP/UDP          |
+-----------------------------+
```

The HDLC codec does **not** parse LLC or APDU payloads. The HDLC `Information` field is treated as opaque bytes. In DLMS/COSEM, the LLC PDU is part of the HDLC `Information` field.

## Key Design Decisions

| Area | Decision |
|---|---|
| Language | C++11 |
| Build system | CMake 3.16+ |
| Error handling | status codes |
| Exceptions | not used in public/runtime API paths |
| Target roles | client and server |
| HDLC frame format | Type 3 |
| Segmentation | fully supported |
| Byte stuffing | not used |
| Frame boundary | determined by Format Field length |
| Closing flag | required |
| Payload byte `0x7E` | allowed inside Information field |
| C ABI | separate stable wrapper |
| Tests | GoogleTest |

## Frame Format

Supported frame layout:

```text
Opening flag
Frame format
Destination address
Source address
Control
HCS, if information is present
Information
FCS
Closing flag
```

The opening and closing flags are not included in the HDLC Format Field length.

The stream decoder must use the Format Field length to determine frame size. It must not terminate a frame on the first `0x7E` found after the opening flag, because byte stuffing is not used and `0x7E` may appear inside the Information field.

## Limits

### `maximum_frame_size`

`maximum_frame_size` is the full frame size in bytes, including opening and closing flags.

The maximum possible value is derived from the 11-bit Format Field length:

```text
max_format_field_length = 2047 bytes
maximum_frame_size = max_format_field_length + 2 flags = 2049 bytes
```

Default:

```text
maximum_frame_size = 2049
```

### `maximum_information_field_size`

`maximum_information_field_size` is the maximum number of bytes in the HDLC `Information` field of one frame.

Formula:

```text
maximum_information_field_size =
  maximum_frame_size
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

`FCS` is a trailer, not a header field.

LLC bytes are part of the HDLC `Information` field and must not be subtracted as HDLC overhead.

Examples:

```text
1-byte destination + 1-byte source:
header_size = 2 + 1 + 1 + 1 = 5
maximum_information_field_size = 2049 - 2 - 5 - 2 - 2 = 2038 bytes

4-byte destination + 4-byte source:
header_size = 2 + 4 + 4 + 1 = 11
maximum_information_field_size = 2049 - 2 - 11 - 2 - 2 = 2032 bytes
```

### `maximum_reassembled_information_size`

This limit protects segmented-frame reassembly.

Default:

```text
maximum_reassembled_information_size = 65535
```

The future HDLC session layer may update codec limits after SNRM/UA negotiation.

## Repository Layout

Planned layout:

```text
.
├── CMakeLists.txt
├── include/
│   └── dlms/
│       └── hdlc/
│           ├── hdlc_types.hpp
│           ├── hdlc_error.hpp
│           ├── hdlc_address.hpp
│           ├── hdlc_control.hpp
│           ├── hdlc_crc.hpp
│           ├── hdlc_frame.hpp
│           ├── hdlc_codec.hpp
│           ├── hdlc_stream_decoder.hpp
│           ├── hdlc_segmentation.hpp
│           └── hdlc_c_api.h
├── src/
│   └── hdlc/
│       ├── hdlc_address.cpp
│       ├── hdlc_control.cpp
│       ├── hdlc_crc.cpp
│       ├── hdlc_codec.cpp
│       ├── hdlc_stream_decoder.cpp
│       ├── hdlc_segmentation.cpp
│       └── hdlc_c_api.cpp
├── test/
│   ├── CMakeLists.txt
│   └── hdlc/
│       ├── test_hdlc_address.cpp
│       ├── test_hdlc_control.cpp
│       ├── test_hdlc_crc.cpp
│       ├── test_hdlc_codec.cpp
│       ├── test_hdlc_stream_decoder.cpp
│       ├── test_hdlc_segmentation.cpp
│       ├── test_hdlc_c_api.cpp
│       └── test_hdlc_vectors.cpp
└── docs/
    ├── 00_hdlc_requirements.md
    ├── 01_hdlc_codec_api.md
    ├── 02_hdlc_c_api.md
    ├── 03_hdlc_segmentation.md
    ├── 04_hdlc_session_requirements.md
    └── 05_hdlc_test_plan.md
```

## Build

Configure:

```bash
cmake -S . -B build -DDLMS_BUILD_TESTS=ON
```

Build:

```bash
cmake --build build
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

## CMake Options

Planned options:

```text
DLMS_BUILD_TESTS       Build GoogleTest tests
DLMS_BUILD_C_API       Build stable C ABI wrapper
DLMS_USE_SYSTEM_GTEST  Use system-installed GoogleTest
```

Example:

```bash
cmake -S . -B build \
  -DDLMS_BUILD_TESTS=ON \
  -DDLMS_BUILD_C_API=ON \
  -DDLMS_USE_SYSTEM_GTEST=OFF
```

## Error Handling

The library uses status codes only.

Example status model:

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

No public/runtime API path should throw exceptions.

## C++ API Direction

The library provides two API styles.

Strict no-allocation API:

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

The no-allocation API is the primary API. STL-based APIs are convenience wrappers.

## C ABI Direction

The C ABI is a separate stable wrapper over the C++ implementation.

Rules:

- `extern "C"`
- no C++ types in public C headers
- opaque handles for stateful objects
- fixed-width integer types
- caller-provided buffers
- status codes only
- stable enum values

Example:

```c
dlms_hdlc_status_t dlms_hdlc_encode_frame(
  const dlms_hdlc_frame_t* frame,
  const dlms_hdlc_limits_t* limits,
  uint8_t* output,
  size_t output_size,
  size_t* written_size);
```

## Segmentation

The codec fully supports HDLC segmentation.

Encoder-side behavior:

```text
if information_size <= maximum_information_field_size:
  produce one frame
  segmented = false

if information_size > maximum_information_field_size:
  produce N frames
  all non-final frames: segmented = true
  final frame: segmented = false
```

Decoder-side reassembly behavior:

```text
segmented = true:
  append information
  return SegmentationIncomplete

segmented = false and pending sequence exists:
  append information
  return completed frame

segmented = false and no pending sequence exists:
  return current frame as completed
```

The reassembler must check:

- destination address consistency
- source address consistency
- compatible frame type
- maximum reassembled information size
- invalid start of a new sequence before completing the previous one

Full sequence-number validation belongs to the future session layer.

## Future HDLC Session Layer

The session layer is not part of v1.

Future responsibilities:

- client/server role behavior
- SNRM generation
- UA parsing
- negotiated limits
- window size
- I-frame send sequence `N(S)`
- I-frame receive sequence `N(R)`
- Poll/Final handling
- RR/RNR handling
- DISC/UA close sequence
- timeouts
- retransmission
- duplicate frame detection
- segmentation policy

The codec must not own session state.

## Test Plan

The test suite must cover:

- CRC/HCS/FCS
- address encoding and decoding
- control field encoding and decoding
- frame encoding and decoding
- stream decoding by Format Field length
- payload containing `0x7E`
- segmentation and reassembly
- C ABI
- real DLMS/COSEM vectors

Required real vectors:

- SNRM request
- UA response
- DISC
- RR
- I-frame carrying LLC payload

## Development Phases

### Phase 0

Documentation and requirements.

Current status:

```text
Done
```

Documents:

```text
docs/00_hdlc_requirements.md
docs/01_hdlc_codec_api.md
docs/02_hdlc_c_api.md
docs/03_hdlc_segmentation.md
docs/04_hdlc_session_requirements.md
docs/05_hdlc_test_plan.md
```

### Phase 1

Project skeleton:

- CMake root project
- include tree
- source tree
- test tree
- empty buildable library
- empty test executable

### Phase 2

Status/error model.

### Phase 3

CRC/HCS/FCS.

### Phase 4

HDLC address codec.

### Phase 5

HDLC control field codec.

### Phase 6

Frame encoder.

### Phase 7

Frame decoder.

### Phase 8

Stream decoder.

### Phase 9

Segmentation and reassembly.

### Phase 10

Stable C ABI.

### Phase 11

Real DLMS/COSEM vectors.

## License

Not selected yet.

Before the first public release, choose and add a license file, for example:

- MIT
- Apache-2.0
- BSD-2-Clause
- BSD-3-Clause
