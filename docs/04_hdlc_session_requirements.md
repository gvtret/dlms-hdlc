# 04. HDLC Session Layer Requirements

## 1. Scope

The HDLC session layer is implemented as a transport-independent state machine
on top of the codec.

The session layer consumes decoded HDLC frames and builds encoded HDLC frames.
It does not own serial/TCP I/O, timers, retransmission scheduling, LLC, or APDU
parsing.

## 2. Session Responsibilities

The initial session layer (v1, implemented) is responsible for:

- client/server role behavior;
- SNRM generation;
- UA connect handling;
- I-frame send sequence N(S);
- I-frame receive sequence N(R);
- Poll/Final handling;
- RR generation and receive-sequence validation;
- DISC/UA close sequence;
- endpoint address validation.

The following responsibilities are deferred to v2:

- SNRM/UA parameter parsing and negotiation;
- applying negotiated maximum information field size;
- applying negotiated window size;
- User_Information passthrough in SNRM and DISC frames;
- timeouts;
- retransmission;
- duplicate frame detection;
- segmentation usage policy;
- updating codec limits after SNRM/UA negotiation.

## 3. Codec Responsibilities

The codec must provide:

- frame encode/decode;
- control field encode/decode;
- segmentation bit encode/decode;
- HCS/FCS calculation and validation;
- address encode/decode;
- stream parsing by Format Field length;
- segmentation/reassembly primitives.

The codec must not:

- increment sequence numbers;
- decide when to send RR;
- decide when to retransmit;
- own timers;
- perform SNRM/UA negotiation;
- parse LLC or APDU.

## 4. Limit Negotiation

After SNRM/UA negotiation, the session layer may update:

```text
maximum_frame_size
maximum_information_field_size
maximum_reassembled_information_size
```

The codec must accept limits as runtime configuration.

## 5. Required Codec Compatibility

The codec must support the minimum client/server frame set:

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

## 6. Transport Independence

The session layer will use transport adapters. The codec and session must remain
transport-independent.

Examples of future transports:

- serial port
- TCP tunnel
- test memory stream
- file replay

---

## 7. v2: SNRM/UA Parameter Negotiation

Reference: IEC 62056-46 §6.4.4.4.3.1, Green Book §8.4.5.3.2.

### 7.1 Information Field Format

When the SNRM or UA frame carries negotiation parameters, the Information field
is encoded as follows:

```text
81h              Format Identifier (mandatory when info field present)
80h              Group Identifier  (mandatory when info field present)
<len>            Group length in bytes (count of bytes following)

05h <len> <val>  max_info_field_length-transmit  (primary → secondary)
06h <len> <val>  max_info_field_length-receive   (secondary → primary)
07h 04h <4 bytes> window_size-transmit
08h 04h <4 bytes> window_size-receive
```

Each parameter is optional. Absence of a parameter means the default value
applies. Parameter values use big-endian byte order. The parameter length for
`max_info_field_length` is 1 or 2 bytes; for `window_size` it is 4 bytes.

Example with all parameters at default values:

```text
81 80 12
05 01 80      max_info_tx  = 128 (default)
06 01 80      max_info_rx  = 128 (default)
07 04 00 00 00 01  window_tx = 1 (default)
08 04 00 00 00 01  window_rx = 1 (default)
```

### 7.2 Default Values

```text
max_info_field_length  = 128 bytes (0x80)
window_size            = 1
```

### 7.3 Maximum Values

```text
max_info_field_length  = 2030 bytes  (Green Book §8.4.3.2)
window_size            = 7           (3-bit sequence numbers, ISO 13239 §5.4.2)
```

### 7.4 Negotiation Rule

The secondary station (server) selects the minimum of the proposed value and the
locally supported value:

```text
result = min(proposed_value, locally_supported_value)
```

The UA frame must include the negotiated values in its Information field. Parameters
whose result equals the default value may be omitted from the UA response.

### 7.5 SNRM with Information Field (server side)

When the server receives a SNRM with an Information field:

1. Parse parameters 05h–08h.
2. Apply negotiation rule per 7.4.
3. Store negotiated limits for the established connection.
4. Call `BuildConnectResponse`; the UA must carry the negotiated parameters.
5. Update codec limits via `HdlcCodecLimits` after UA is sent.

If the SNRM Information field contains unrecognised parameter identifiers, the
server shall reject the connection with a DM frame.

### 7.6 SNRM without Information Field (client side)

When the client sends SNRM without an Information field, both sides use default
values. The server UA may or may not carry an Information field in this case.

### 7.7 Session API Extension

The session options must expose locally supported limits so the session can
perform negotiation:

```cpp
struct HdlcSessionNegotiationLimits
{
  std::size_t maxInformationFieldLengthTransmit;
  std::size_t maxInformationFieldLengthReceive;
  std::uint8_t windowSizeTransmit;
  std::uint8_t windowSizeReceive;
};
```

After a successful SNRM/UA exchange, the session must expose the negotiated
result and update the codec limits accordingly.

### 7.8 Conformance Tests

| Test | Requirement |
|---|---|
| HDLC_NDM2NRM_P1 | IUT answers UA with MaximumInformationFieldLength ≤ 2030 |
| HDLC_NDM2NRM_P2 | IUT answers UA with WindowSize within 1–7 |

---

## 8. v2: Window Size > 1 (Sliding Window)

Reference: IEC 62056-46 §6.4.4.4.3.5, ISO 13239 §5.4.2.

### 8.1 State Variables

```text
V(S)  send state variable  — sequence number of the next I-frame to transmit
V(R)  receive state variable — next expected I-frame sequence number
V(A)  acknowledge state variable — last acknowledged N(S) + 1
```

The current session (v1) tracks only `sendSequence_` (= V(S)) and
`receiveSequence_` (= V(R)) and implicitly uses window = 1, so V(A) = V(S) − 1.

### 8.2 Sliding Window Rules

```text
outstanding I-frames = V(S) - V(A)  (mod 8)
may send next I-frame if outstanding_frames < window_size
RR N(R) acknowledges all I-frames with N(S) < N(R)
V(A) advances to N(R) on receipt of RR/I with valid N(R)
```

### 8.3 Session API Extension

```cpp
bool CanSendInformationFrame() const;  // true when outstanding < window
std::uint8_t AcknowledgeSequence() const;  // V(A)
```

`BuildInformationFrame` must check `CanSendInformationFrame()` before encoding.

---

## 9. v2: User_Information Passthrough in SNRM and DISC

Reference: IEC 62056-7-6 §6.

### 9.1 Purpose

The 3-layer CO HDLC profile allows COSEM-OPEN and COSEM-RELEASE to pass
`User_Information` through the HDLC data link layer via the SNRM and DISC
Information fields respectively. The HDLC session treats this as opaque bytes
appended after any negotiation parameters.

### 9.2 SNRM with User_Information

```text
Information field layout:
  [81 80 <len> <negotiation params>]   optional, as per §7.1
  [<user_data bytes>]                  optional, opaque
```

The session must accept an optional `user_information` buffer in
`BuildConnectRequest` and must forward it to the upper layer on the server side
via `ReceiveFrame`.

### 9.3 DISC with User_Information

Analogously, `BuildDisconnectRequest` must accept an optional
`user_information` buffer.

### 9.4 Session API Extension

```cpp
HdlcStatus BuildConnectRequest(
  const std::uint8_t* userInformation,
  std::size_t userInformationSize,
  std::vector<std::uint8_t>& output);

HdlcStatus BuildDisconnectRequest(
  const std::uint8_t* userInformation,
  std::size_t userInformationSize,
  std::vector<std::uint8_t>& output);
```

The v1 overloads without `userInformation` remain valid and produce frames
without a User_Information subfield.

---

## 10. v2: Timeouts and Retransmission

Reference: IEC 62056-46 §6.4.4.3.3, §6.4.4.4.3.1.

The session does not own timers. The transport adapter or the application layer
must:

1. Start a response timeout after sending SNRM, DISC, or each I-frame with P=1.
2. Call `BuildConnectRequest` / `BuildInformationFrame` again on timeout to
   retransmit.
3. Count retries; abort after `MAX_NB_OF_RETRIES` (application-defined).

The session must expose a `Reset()` method to return to `Disconnected` without
requiring a full DISC/UA exchange (for abort conditions).

---

## 11. v2: Duplicate Frame Detection

Reference: ISO 13239 §5.4.

The session must discard I-frames whose N(S) does not equal V(R). The v1
implementation already returns `InvalidControlField` for wrong N(S), which
causes the transport to discard the frame. Full duplicate detection (storing
and discarding re-delivered frames within a window) is deferred.

---

## 12. v2: Test Plan Additions

### 12.1 SNRM/UA Negotiation Tests

```text
Session_SnrmWithDefaultParameters_ClientSendsNoInfoField
Session_SnrmWithMaxInfoFieldLength_ServerNegotiates
Session_SnrmWithWindowSize_ServerNegotiates
Session_SnrmWithBothParameters_ServerNegotiates
Session_SnrmWithUnrecognisedParameter_ServerRejectsDm
Session_UaCarriesNegotiatedValues
Session_CodecLimitsUpdatedAfterNegotiation
```

### 12.2 Window Size Tests

```text
Session_WindowSize1_CannotSendSecondFrameBeforeRr
Session_WindowSize3_CanSendThreeFramesBeforeRr
Session_RrAdvancesAcknowledgeSequence
Session_WindowExhaustedBlocksSend
```

### 12.3 User_Information Tests

```text
Session_SnrmWithUserInformation_ServerReceivesPayload
Session_DiscWithUserInformation_PeerReceivesPayload
```

### 12.4 Real Vectors

Add real SNRM/UA frames with negotiation parameters from Green Book §8.4.5.3.2
examples to the existing `test_hdlc_real_vectors.cpp`.
