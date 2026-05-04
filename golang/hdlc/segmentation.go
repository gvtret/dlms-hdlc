package hdlc

const defaultMaximumInformationFieldSizeForSegmentation = uint(2030)

// SegmentationOptions configures a Segmenter.
type SegmentationOptions struct {
	// Limits are used to determine the per-frame Information chunk size.
	Limits CodecLimits
}

// Segmenter splits an Information payload into HDLC frame-sized chunks.
// It copies metadata from a base frame and produces one or more output frames.
// Non-final chunks have Segmented=true; the final chunk has Segmented=false.
type Segmenter struct {
	options SegmentationOptions
}

// NewSegmenter constructs a segmenter with the given options.
func NewSegmenter(opts SegmentationOptions) *Segmenter {
	return &Segmenter{options: opts}
}

func (s *Segmenter) effectiveMaxInfo() uint {
	if s.options.Limits.MaximumInformationFieldSize == 0 {
		return defaultMaximumInformationFieldSizeForSegmentation
	}
	return s.options.Limits.MaximumInformationFieldSize
}

// Segment splits information into one or more frames derived from baseFrame.
// The base frame's addresses and control field are copied to every output frame.
// When information fits in a single chunk, one frame with Segmented=false is
// returned. Larger payloads produce N frames; all non-final frames have
// Segmented=true and the final frame has Segmented=false.
func (s *Segmenter) Segment(baseFrame Frame, information []byte) ([]Frame, error) {
	maxInfo := s.effectiveMaxInfo()
	if maxInfo == 0 {
		return nil, StatusInvalidArgument
	}

	if uint(len(information)) <= maxInfo {
		f := baseFrame
		f.Segmented = false
		if len(information) > 0 {
			f.Information = make([]byte, len(information))
			copy(f.Information, information)
		} else {
			f.Information = nil
		}
		return []Frame{f}, nil
	}

	var frames []Frame
	offset := 0
	for offset < len(information) {
		remaining := len(information) - offset
		chunkSize := int(maxInfo)
		if remaining < chunkSize {
			chunkSize = remaining
		}
		hasMore := offset+chunkSize < len(information)

		chunk := make([]byte, chunkSize)
		copy(chunk, information[offset:offset+chunkSize])

		f := baseFrame
		f.Segmented = hasMore
		f.Information = chunk
		frames = append(frames, f)
		offset += chunkSize
	}
	return frames, nil
}

// Reassembler combines decoded segmented HDLC frames into a complete
// Information payload. It validates destination address, source address,
// compatible frame kind, and accumulated Information size against the
// configured limits. It does not perform session-layer sequence validation,
// retransmission, or timeout behavior.
type Reassembler struct {
	limits     CodecLimits
	hasPending bool
	pending    Frame
}

// NewReassembler constructs a reassembler with the given codec limits.
// MaximumReassembledInformationSize in limits caps the total accumulated
// Information across all segments in a sequence.
func NewReassembler(limits CodecLimits) *Reassembler {
	return &Reassembler{limits: limits}
}

func sameAddress(a, b Address) bool {
	return a.RawValue() == b.RawValue() && a.EncodedSize() == b.EncodedSize()
}

// Push feeds one decoded frame into the reassembly state machine.
// A non-segmented frame with no pending sequence completes immediately.
// A segmented frame starts or continues a sequence and returns
// StatusSegmentationIncomplete until the final non-segmented frame arrives.
// Returns (completedFrame, true, nil) when the sequence is complete,
// (Frame{}, false, StatusSegmentationIncomplete) when more frames are needed,
// or (Frame{}, false, err) on a validation or overflow error.
func (r *Reassembler) Push(frame Frame) (Frame, bool, error) {
	maxReassembled := effectiveMaximumReassembledInformationSize(r.limits)

	if !r.hasPending {
		if uint(len(frame.Information)) > maxReassembled {
			return Frame{}, false, StatusSegmentationOverflow
		}
		if !frame.Segmented {
			return frame, true, nil
		}
		r.pending = frame
		r.pending.Information = make([]byte, len(frame.Information))
		copy(r.pending.Information, frame.Information)
		r.hasPending = true
		return Frame{}, false, StatusSegmentationIncomplete
	}

	if !sameAddress(r.pending.Destination, frame.Destination) ||
		!sameAddress(r.pending.Source, frame.Source) ||
		r.pending.Control.FrameKind() != frame.Control.FrameKind() {
		r.Reset()
		return Frame{}, false, StatusSegmentationError
	}

	if uint(len(r.pending.Information)) > maxReassembled-uint(len(frame.Information)) {
		r.Reset()
		return Frame{}, false, StatusSegmentationOverflow
	}

	r.pending.Information = append(r.pending.Information, frame.Information...)

	if frame.Segmented {
		return Frame{}, false, StatusSegmentationIncomplete
	}

	completed := r.pending
	completed.Segmented = false
	r.Reset()
	return completed, true, nil
}

// Reset discards any pending segmented sequence and returns the reassembler to
// its initial empty state.
func (r *Reassembler) Reset() {
	r.hasPending = false
	r.pending = Frame{}
}
