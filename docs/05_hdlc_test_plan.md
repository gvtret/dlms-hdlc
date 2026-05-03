# 05. HDLC Test Plan

## 1. Test Framework

GoogleTest is mandatory.

The CMake project must support:

```text
DLMS_BUILD_TESTS=ON
DLMS_USE_SYSTEM_GTEST=ON/OFF
```

## 2. CRC Tests

Required tests:

- CRC known vector for SNRM;
- CRC known vector for UA;
- valid HCS;
- invalid HCS;
- valid FCS;
- invalid FCS;
- table-based and bitwise implementations produce the same result.

## 3. Address Tests

Required tests:

- 1-byte address encode/decode;
- 2-byte address encode/decode;
- 4-byte address encode/decode;
- truncated address;
- invalid extension bit;
- client address helper;
- server logical/physical address helper.

## 4. Control Field Tests

Required tests:

- I-frame;
- RR;
- RNR;
- REJ;
- SREJ;
- SNRM;
- UA;
- DISC;
- DM;
- FRMR;
- UI;
- Poll/Final bit;
- sequence numbers;
- encode/decode roundtrip.

## 5. Frame Codec Tests

Required tests:

- encode SNRM;
- decode SNRM;
- encode UA;
- decode UA;
- encode DISC;
- decode DISC;
- encode RR;
- decode RR;
- encode I-frame without segmentation;
- encode I-frame with segmentation bit;
- invalid format type;
- invalid length;
- invalid HCS;
- invalid FCS;
- frame with payload byte `0x7E`;
- maximum frame size;
- information field too large.

## 6. Stream Decoder Tests

Required tests:

- full frame in one push;
- byte-by-byte input;
- multiple frames in one buffer;
- frame with payload byte `0x7E`;
- noise before opening flag with ignore policy;
- noise before opening flag with error policy;
- invalid length;
- missing closing flag;
- wrong closing flag;
- frame too large;
- reset after error.

## 7. Segmentation Tests

Required tests:

- single-frame payload;
- multiple-frame payload;
- exact boundary payload;
- reassemble single frame;
- reassemble multiple frames;
- address mismatch;
- control mismatch;
- reassembly overflow;
- new segmented sequence before previous completion.

## 8. C ABI Tests

Required tests:

- encode frame through C API;
- decode frame through C API;
- output buffer too small;
- null argument validation;
- stream decoder create/destroy/reset;
- reassembler create/destroy/reset;
- no crash on invalid inputs.

## 9. Real DLMS/COSEM Vectors

The test suite must include real hex vectors for:

- SNRM request;
- UA response;
- DISC;
- RR;
- I-frame carrying LLC payload.

Known vectors must be stored in a dedicated test file.
