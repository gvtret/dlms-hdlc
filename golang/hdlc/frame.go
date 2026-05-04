package hdlc

// Frame represents an HDLC Type 3 frame with an owned Information field.
// It is the unified frame type used by all codec and session APIs in this
// package. The Information slice is nil when the frame carries no payload.
type Frame struct {
	// Segmented is the HDLC Frame Format segmentation bit.
	// True means more segments follow; false means this is the final segment
	// or the frame is not segmented.
	Segmented bool

	// Destination is the destination HDLC address.
	Destination Address
	// Source is the source HDLC address.
	Source Address

	// Control is the parsed HDLC control field.
	Control Control

	// Information holds the opaque Information field bytes, or nil when empty.
	Information []byte
}
