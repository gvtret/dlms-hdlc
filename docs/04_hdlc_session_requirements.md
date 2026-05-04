# 04. HDLC Session Layer Requirements

## 1. Scope

The HDLC session layer is implemented as a transport-independent state machine
on top of the codec.

The session layer consumes decoded HDLC frames and builds encoded HDLC frames.
It does not own serial/TCP I/O, timers, retransmission scheduling, LLC, or APDU
parsing.

## 2. Session Responsibilities

The initial session layer is responsible for:

- client/server role behavior;
- SNRM generation;
- UA connect handling;
- I-frame send sequence N(S);
- I-frame receive sequence N(R);
- Poll/Final handling;
- RR generation and receive-sequence validation;
- DISC/UA close sequence;
- endpoint address validation.

The following responsibilities remain future work:

- SNRM/UA parameter parsing;
- negotiated maximum information field size;
- negotiated window size;
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
