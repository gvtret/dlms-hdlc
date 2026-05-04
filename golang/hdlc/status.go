package hdlc

import "fmt"

// Status is the result code returned by all HDLC codec operations.
// The library uses status codes instead of panics in public runtime paths.
// Numeric values are part of the stable API contract and are mirrored by the
// C ABI status enum in hdlc_c_api.h.
type Status int

const (
	// StatusOk means the operation completed successfully.
	StatusOk Status = 0

	// StatusNeedMoreData means more input bytes are required before a complete
	// frame can be decoded.
	StatusNeedMoreData Status = 1
	// StatusOutputBufferTooSmall means the caller-provided output or information
	// buffer is too small to hold the result.
	StatusOutputBufferTooSmall Status = 2

	// StatusInvalidArgument means a pointer, size, option, or argument
	// combination is invalid.
	StatusInvalidArgument Status = 3
	// StatusInvalidFlag means the opening or closing HDLC flag is missing or
	// does not equal 0x7E.
	StatusInvalidFlag Status = 4
	// StatusInvalidFrameFormat means the Frame Format field does not contain
	// a supported HDLC Type 3 format identifier.
	StatusInvalidFrameFormat Status = 5
	// StatusInvalidFrameType means the control field encodes a syntactically
	// unsupported frame type.
	StatusInvalidFrameType Status = 6
	// StatusInvalidFrameLength means the Frame Format length is inconsistent,
	// too small, or otherwise invalid.
	StatusInvalidFrameLength Status = 7

	// StatusInvalidAddress means a destination or source HDLC address field
	// is malformed or out of range.
	StatusInvalidAddress Status = 8
	// StatusInvalidControlField means the control field is malformed or
	// carries an unsupported unnumbered command.
	StatusInvalidControlField Status = 9

	// StatusInvalidHeaderChecksum means the Header Check Sequence verification
	// failed for a frame that carries an Information field.
	StatusInvalidHeaderChecksum Status = 10
	// StatusInvalidFrameChecksum means the Frame Check Sequence verification
	// failed.
	StatusInvalidFrameChecksum Status = 11

	// StatusFrameTooLarge means the frame exceeds the configured or
	// format-derived maximum frame size.
	StatusFrameTooLarge Status = 12
	// StatusInformationFieldTooLarge means the Information field exceeds the
	// configured per-frame information size limit.
	StatusInformationFieldTooLarge Status = 13

	// StatusSegmentationError means the segmented-frame sequence is
	// inconsistent or cannot be continued (e.g. address mismatch).
	StatusSegmentationError Status = 14
	// StatusSegmentationIncomplete means the segmented sequence is valid so far
	// but more frames are required to complete reassembly.
	StatusSegmentationIncomplete Status = 15
	// StatusSegmentationOverflow means the reassembled information would exceed
	// the configured reassembly size limit.
	StatusSegmentationOverflow Status = 16

	// StatusUnsupportedFrame means the frame is valid HDLC but outside the
	// supported codec or session feature set.
	StatusUnsupportedFrame Status = 17
	// StatusUnsupportedAddress means the address is valid HDLC but outside the
	// supported address model.
	StatusUnsupportedAddress Status = 18
	// StatusUnsupportedFeature means the requested feature is intentionally
	// outside this codec layer.
	StatusUnsupportedFeature Status = 19

	// StatusInternalError means an unexpected internal failure occurred,
	// typically an allocation failure inside a convenience API.
	StatusInternalError Status = 20
)

func (s Status) Error() string {
	switch s {
	case StatusOk:
		return "ok"
	case StatusNeedMoreData:
		return "need more data"
	case StatusOutputBufferTooSmall:
		return "output buffer too small"
	case StatusInvalidArgument:
		return "invalid argument"
	case StatusInvalidFlag:
		return "invalid flag"
	case StatusInvalidFrameFormat:
		return "invalid frame format"
	case StatusInvalidFrameType:
		return "invalid frame type"
	case StatusInvalidFrameLength:
		return "invalid frame length"
	case StatusInvalidAddress:
		return "invalid address"
	case StatusInvalidControlField:
		return "invalid control field"
	case StatusInvalidHeaderChecksum:
		return "invalid header checksum"
	case StatusInvalidFrameChecksum:
		return "invalid frame checksum"
	case StatusFrameTooLarge:
		return "frame too large"
	case StatusInformationFieldTooLarge:
		return "information field too large"
	case StatusSegmentationError:
		return "segmentation error"
	case StatusSegmentationIncomplete:
		return "segmentation incomplete"
	case StatusSegmentationOverflow:
		return "segmentation overflow"
	case StatusUnsupportedFrame:
		return "unsupported frame"
	case StatusUnsupportedAddress:
		return "unsupported address"
	case StatusUnsupportedFeature:
		return "unsupported feature"
	case StatusInternalError:
		return "internal error"
	default:
		return fmt.Sprintf("unknown status %d", int(s))
	}
}
