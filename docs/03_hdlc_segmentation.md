# 03. HDLC Segmentation and Reassembly Requirements

## 1. Scope

This document defines full codec-layer support for HDLC segmentation.

The codec does not implement session state, retransmission, or timeout handling.

## 2. Segmentation Bit

Frame Format Type 3 contains:

```text
format type: 4 bits
segmentation bit: 1 bit
frame length: 11 bits
```

The codec must encode and decode the segmentation bit.

## 3. Encoder-Side Segmentation

The segmenter splits one information payload into multiple HDLC frames.

Rules:

```text
if information_size <= maximum_information_field_size:
  produce one frame
  segmented = false

if information_size > maximum_information_field_size:
  produce N frames
  all non-final frames: segmented = true
  final frame: segmented = false
```

## 4. Maximum Information Field Size

The per-frame information limit is calculated as:

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

LLC bytes are part of the information field and must not be subtracted as HDLC overhead.

## 5. Reassembly

The reassembler combines a segmented sequence into one completed information payload.

Rules:

```text
segmented = true:
  append information
  return SegmentationIncomplete

segmented = false and no pending sequence:
  return current frame as completed

segmented = false and pending sequence exists:
  append information
  return completed frame
```

## 6. Reassembly Limit

The maximum accumulated information size is:

```text
maximum_reassembled_information_size = 65535
```

The reassembler must return `SegmentationOverflow` if accumulation would exceed this limit.

## 7. Reassembly Consistency Checks

The reassembler must verify:

- destination address is unchanged during a segmented sequence;
- source address is unchanged during a segmented sequence;
- frame kind is compatible;
- a new segmented sequence is not started before the previous one is completed;
- accumulated size does not exceed `maximum_reassembled_information_size`.

Full sequence-number validation belongs to the future session layer.

## 8. API Sketch

```cpp
struct HdlcSegmentationOptions
{
  HdlcCodecLimits limits;
};
```

```cpp
class HdlcSegmenter
{
public:
  explicit HdlcSegmenter(const HdlcSegmentationOptions& options);

  HdlcStatus SegmentInformation(
    const HdlcFrame& baseFrame,
    const std::uint8_t* information,
    std::size_t informationSize,
    std::vector<HdlcFrameBuffer>& outputFrames);
};
```

```cpp
class HdlcReassembler
{
public:
  explicit HdlcReassembler(const HdlcCodecLimits& limits);

  HdlcStatus PushFrame(
    const HdlcFrameBuffer& frame,
    HdlcFrameBuffer& completedFrame,
    bool& hasCompletedFrame);

  void Reset();
};
```

## 9. Out of Scope

The segmentation module must not:

- send RR/RNR;
- perform retransmission;
- manage timers;
- negotiate parameters;
- own HDLC sequence numbers;
- parse LLC or APDU.
