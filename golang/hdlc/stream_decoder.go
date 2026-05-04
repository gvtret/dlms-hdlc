package hdlc

import "encoding/binary"

// NoisePolicy controls how bytes received before the next HDLC opening flag
// are handled.
type NoisePolicy int

const (
	// NoisePolicyIgnore discards leading bytes silently until an opening flag
	// (0x7E) is found.
	NoisePolicyIgnore NoisePolicy = iota
	// NoisePolicyReportError returns StatusInvalidFlag when any byte is received
	// before an opening flag.
	NoisePolicyReportError
)

// StreamDecoderOptions configures a StreamDecoder.
type StreamDecoderOptions struct {
	// Limits are the frame and Information size limits used for length checks
	// and frame decoding.
	Limits CodecLimits
	// NoisePolicy controls how bytes before the opening flag are handled.
	NoisePolicy NoisePolicy
}

// StreamDecoder accepts arbitrary byte chunks and extracts complete HDLC frames
// using the Frame Format length field. It does not terminate frames on payload
// bytes equal to 0x7E; the closing flag is checked only at the length-derived
// frame boundary.
type StreamDecoder struct {
	options StreamDecoderOptions
	buffer  []byte
}

// NewStreamDecoder constructs a stream decoder with the given options.
func NewStreamDecoder(opts StreamDecoderOptions) *StreamDecoder {
	return &StreamDecoder{options: opts}
}

// Push feeds a byte chunk into the decoder and returns all complete frames
// extracted from the accumulated input. Returns StatusNeedMoreData (with an
// empty slice) when no full frame is available yet. Returns a non-nil error
// when a frame validation or noise-policy error is encountered; the internal
// buffer is cleared on error.
func (d *StreamDecoder) Push(data []byte) ([]Frame, error) {
	d.buffer = append(d.buffer, data...)

	var frames []Frame
	for {
		// Find opening flag.
		flagIdx := -1
		for i, b := range d.buffer {
			if b == hdlcFlag {
				flagIdx = i
				break
			}
		}
		if flagIdx < 0 {
			if len(d.buffer) > 0 && d.options.NoisePolicy == NoisePolicyReportError {
				d.Reset()
				return nil, StatusInvalidFlag
			}
			d.buffer = d.buffer[:0]
			if len(frames) == 0 {
				return nil, StatusNeedMoreData
			}
			return frames, nil
		}

		if flagIdx > 0 {
			if d.options.NoisePolicy == NoisePolicyReportError {
				d.Reset()
				return nil, StatusInvalidFlag
			}
			d.buffer = d.buffer[flagIdx:]
		}

		if len(d.buffer) < 3 {
			if len(frames) == 0 {
				return nil, StatusNeedMoreData
			}
			return frames, nil
		}

		ff := binary.BigEndian.Uint16(d.buffer[1:])
		if ff&frameFormatTypeMask != frameFormatType3 {
			d.Reset()
			return nil, StatusInvalidFrameFormat
		}

		formatFieldLength := int(ff & 0x07ff)
		fullFrameSize := formatFieldLength + 2

		if formatFieldLength < 6 {
			d.Reset()
			return nil, StatusInvalidFrameLength
		}
		if uint(fullFrameSize) > effectiveMaximumFrameSize(d.options.Limits) {
			d.Reset()
			return nil, StatusFrameTooLarge
		}
		if len(d.buffer) < fullFrameSize {
			if len(frames) == 0 {
				return nil, StatusNeedMoreData
			}
			return frames, nil
		}

		frame, _, err := DecodeFrameFromBuffer(d.buffer[:fullFrameSize], d.options.Limits, nil)
		if err != nil {
			d.Reset()
			return nil, err
		}

		frames = append(frames, frame)
		d.buffer = d.buffer[fullFrameSize:]
	}
}

// Reset discards all buffered partial input and returns the decoder to its
// initial empty state.
func (d *StreamDecoder) Reset() {
	d.buffer = d.buffer[:0]
}
