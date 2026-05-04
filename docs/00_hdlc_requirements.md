# 00. HDLC Codec Requirements

## 1. Scope

This document defines requirements for a portable C++11 HDLC library for DLMS/COSEM.

The library implements HDLC Frame Format Type 3 encoding and decoding. It is part of a future DLMS/COSEM framework that will also include:

- LLC codec
- WRAPPER codec
- APDU codec
- HDLC session layer

Version 1 implements the codec layer and an initial transport-independent HDLC
session state machine.

## 2. Fixed Design Decisions

| Area | Decision |
|---|---|
| Language | C++11 |
| Build system | CMake 3.16+ |
| Error handling | Status codes only |
| Exceptions | Not used in public/runtime API paths |
| Target roles | Client and server |
| HDLC frame format | Type 3 |
| Segmentation | Fully supported in codec layer |
| Byte stuffing | Not used |
| Frame boundary | Determined by Format Field length |
| Closing flag | Required |
| Payload byte `0x7E` | Allowed inside information field |
| Session layer | Transport-independent state machine |
| C ABI | Stable separate C ABI layer |
| Tests | GoogleTest |

## 3. Layering

The HDLC codec must not parse LLC or APDU payloads.

Expected HDLC-based stack:

```text
APDU codec
LLC codec
HDLC session layer
HDLC codec
Transport
```

The HDLC codec treats the HDLC `Information` field as opaque bytes. In DLMS/COSEM, the LLC PDU is part of the HDLC `Information` field.

## 4. Frame Format

The supported frame layout is:

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

## 5. Maximum Frame Size

`maximum_frame_size` is the full frame size in bytes, including the opening flag and closing flag.

The maximum possible value is derived from the 11-bit Format Field length:

```text
max_format_field_length = 2047 bytes
maximum_frame_size = max_format_field_length + 2 flags = 2049 bytes
```

Therefore the default codec limit is:

```text
maximum_frame_size = 2049
```

The codec must reject frames larger than `maximum_frame_size`.

## 6. Header Size

For information-bearing frames, the HDLC header size is defined as:

```text
header_size =
  2 bytes frame format
+ destination address size
+ source address size
+ 1 byte control
```

`FCS` is not part of the header. It is a trailer and must be subtracted separately when calculating the maximum information field size.

## 7. Maximum Information Field Size

`maximum_information_field_size` is the maximum number of bytes in the HDLC `Information` field of one HDLC frame.

Because LLC is part of the HDLC `Information` field, LLC bytes must not be subtracted from HDLC overhead.

Formula:

```text
maximum_information_field_size =
  maximum_frame_size
- 2 bytes flags
- header_size
- 2 bytes HCS
- 2 bytes FCS
```

For 1-byte destination and source addresses:

```text
header_size = 2 + 1 + 1 + 1 = 5
maximum_information_field_size = 2049 - 2 - 5 - 2 - 2 = 2038 bytes
```

For 4-byte destination and source addresses:

```text
header_size = 2 + 4 + 4 + 1 = 11
maximum_information_field_size = 2049 - 2 - 11 - 2 - 2 = 2032 bytes
```

The codec must calculate the actual maximum information size from the selected address sizes.

## 8. Maximum Reassembled Information Size

`maximum_reassembled_information_size` limits the accumulated information payload after segmented-frame reassembly.

Fixed default:

```text
maximum_reassembled_information_size = 65535
```

The reassembler must reject any segmented sequence that exceeds this value.

## 9. Runtime Negotiation

Limits may be changed after SNRM/UA negotiation by the HDLC session layer once
parameter parsing is implemented.

The codec itself does not perform SNRM/UA negotiation, but it exposes
configuration points so the session layer can update:

```text
maximum_frame_size
maximum_information_field_size
maximum_reassembled_information_size
```

## 10. Validation Requirements

The codec must validate:

- opening flag
- closing flag
- frame format type
- Format Field length
- segmentation bit
- destination address
- source address
- control field
- HCS when information is present
- FCS
- configured maximum frame size
- configured maximum information size
- configured maximum reassembled information size

## 11. Out of Scope for Version 1

The following are not implemented in v1:

- full SNRM/UA parameter negotiation logic
- timeout handling
- retransmission
- transport layer
- LLC codec
- WRAPPER codec
- APDU codec
- APDU block transfer
- security and ciphering
