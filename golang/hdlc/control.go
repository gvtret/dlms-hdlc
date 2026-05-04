package hdlc

// FrameKind is the high-level category encoded by the HDLC control field.
type FrameKind int

const (
	// FrameKindInformation is an I-frame that carries user information and
	// three-bit send/receive sequence numbers.
	FrameKindInformation FrameKind = iota
	// FrameKindSupervisory is an S-frame such as RR, RNR, REJ, or SREJ.
	FrameKindSupervisory
	// FrameKindUnnumbered is a U-frame such as SNRM, UA, DISC, DM, FRMR, or UI.
	FrameKindUnnumbered
)

// SupervisoryKind is the supervisory command/response kind encoded in an S-frame.
type SupervisoryKind int

const (
	// SupervisoryReceiveReady is Receive Ready (RR).
	SupervisoryReceiveReady SupervisoryKind = iota
	// SupervisoryReceiveNotReady is Receive Not Ready (RNR).
	SupervisoryReceiveNotReady
	// SupervisoryReject is Reject (REJ).
	SupervisoryReject
	// SupervisorySelectiveReject is Selective Reject (SREJ).
	SupervisorySelectiveReject
)

// UnnumberedKind is the unnumbered command/response kind encoded in a U-frame.
type UnnumberedKind int

const (
	// UnnumberedSnrm is Set Normal Response Mode (SNRM), control byte 0x93.
	UnnumberedSnrm UnnumberedKind = iota
	// UnnumberedUa is Unnumbered Acknowledgement (UA), control byte 0x73.
	UnnumberedUa
	// UnnumberedDisc is Disconnect (DISC), control byte 0x53.
	UnnumberedDisc
	// UnnumberedDm is Disconnected Mode (DM), control byte 0x1F.
	UnnumberedDm
	// UnnumberedFrmr is Frame Reject (FRMR), control byte 0x97.
	UnnumberedFrmr
	// UnnumberedUi is Unnumbered Information (UI), control byte 0x13.
	UnnumberedUi
)

// Control holds a parsed HDLC control field byte.
// It preserves the raw wire byte and exposes frame kind, Poll/Final bit, and
// sequence-number accessors for I- and S-frames.
type Control struct {
	raw uint8
}

func isSupportedUnnumbered(value uint8) bool {
	switch value & 0xef {
	case 0x83, 0x63, 0x43, 0x0f, 0x87, 0x03:
		return true
	}
	return false
}

// ControlFromByte decodes a raw HDLC control byte.
// Returns StatusInvalidControlField when the byte carries an unsupported
// unnumbered command.
func ControlFromByte(value byte) (Control, error) {
	if value&0x01 == 0 {
		return Control{raw: value}, nil
	}
	if value&0x03 == 0x01 {
		return Control{raw: value}, nil
	}
	if isSupportedUnnumbered(value) {
		return Control{raw: value}, nil
	}
	return Control{}, StatusInvalidControlField
}

// ControlFromRaw constructs a Control from a raw byte without validation.
// Use only when the byte is known to be valid, for example when building
// session-layer unnumbered frames from fixed constants.
func ControlFromRaw(value byte) Control {
	return Control{raw: value}
}

// Encode returns the raw wire control byte.
func (c Control) Encode() byte { return c.raw }

// FrameKind returns the high-level HDLC frame category: Information,
// Supervisory, or Unnumbered.
func (c Control) FrameKind() FrameKind {
	if c.raw&0x01 == 0 {
		return FrameKindInformation
	}
	if c.raw&0x03 == 0x01 {
		return FrameKindSupervisory
	}
	return FrameKindUnnumbered
}

// PollFinal returns true when the Poll/Final bit (P/F) is set in the control
// field.
func (c Control) PollFinal() bool { return c.raw&0x10 != 0 }

// SupervisoryKind returns the supervisory command kind for S-frames.
// Returns StatusInvalidFrameType for non-supervisory frames.
func (c Control) SupervisoryKind() (SupervisoryKind, error) {
	if c.FrameKind() != FrameKindSupervisory {
		return 0, StatusInvalidFrameType
	}
	switch (c.raw >> 2) & 0x03 {
	case 0:
		return SupervisoryReceiveReady, nil
	case 1:
		return SupervisoryReceiveNotReady, nil
	case 2:
		return SupervisoryReject, nil
	case 3:
		return SupervisorySelectiveReject, nil
	}
	return 0, StatusInvalidControlField
}

// UnnumberedKind returns the unnumbered command/response kind for U-frames.
// Returns StatusInvalidFrameType for non-unnumbered frames and
// StatusInvalidControlField for an unsupported unnumbered command.
func (c Control) UnnumberedKind() (UnnumberedKind, error) {
	if c.FrameKind() != FrameKindUnnumbered {
		return 0, StatusInvalidFrameType
	}
	switch c.raw & 0xef {
	case 0x83:
		return UnnumberedSnrm, nil
	case 0x63:
		return UnnumberedUa, nil
	case 0x43:
		return UnnumberedDisc, nil
	case 0x0f:
		return UnnumberedDm, nil
	case 0x87:
		return UnnumberedFrmr, nil
	case 0x03:
		return UnnumberedUi, nil
	}
	return 0, StatusInvalidControlField
}

// SendSequence returns the three-bit send sequence number N(S) for I-frames.
// Returns zero for frame types that do not carry N(S).
func (c Control) SendSequence() uint8 {
	if c.FrameKind() != FrameKindInformation {
		return 0
	}
	return (c.raw >> 1) & 0x07
}

// ReceiveSequence returns the three-bit receive sequence number N(R) for I- and
// S-frames. Returns zero for U-frames that do not carry N(R).
func (c Control) ReceiveSequence() uint8 {
	if c.FrameKind() == FrameKindUnnumbered {
		return 0
	}
	return (c.raw >> 5) & 0x07
}
