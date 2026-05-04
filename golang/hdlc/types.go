package hdlc

// CodecLimits holds runtime limits used by the frame codec, stream decoder,
// and reassembler. A zero field means "use the codec default" for that limit.
// The HDLC session layer may update these values after SNRM/UA negotiation,
// but the codec itself does not perform negotiation.
type CodecLimits struct {
	// MaximumFrameSize is the maximum complete HDLC frame size in bytes,
	// including both flag bytes. Zero means use the codec default (2049).
	MaximumFrameSize uint
	// MaximumInformationFieldSize is the maximum Information field size accepted
	// in a single HDLC frame. Zero means derive the limit per frame from
	// MaximumFrameSize and the address sizes.
	MaximumInformationFieldSize uint
	// MaximumReassembledInformationSize is the maximum accumulated Information
	// size accepted by the reassembler. Zero means use the default (65535).
	MaximumReassembledInformationSize uint
}

// DefaultCodecLimits returns the codec default safety limits suitable for
// normal DLMS/COSEM HDLC use:
// MaximumFrameSize=2049, MaximumInformationFieldSize=0 (derive per frame),
// MaximumReassembledInformationSize=65535.
func DefaultCodecLimits() CodecLimits {
	return CodecLimits{
		MaximumFrameSize:                  2049,
		MaximumInformationFieldSize:       0,
		MaximumReassembledInformationSize: 65535,
	}
}

func effectiveMaximumFrameSize(limits CodecLimits) uint {
	if limits.MaximumFrameSize == 0 {
		return 2049
	}
	return limits.MaximumFrameSize
}

func effectiveMaximumInformationFieldSize(destSize, srcSize uint, limits CodecLimits) uint {
	if limits.MaximumInformationFieldSize != 0 {
		return limits.MaximumInformationFieldSize
	}
	headerSize := uint(2) + destSize + srcSize + 1
	maxFrame := effectiveMaximumFrameSize(limits)
	overhead := uint(2) + headerSize + 2 + 2
	if maxFrame <= overhead {
		return 0
	}
	return maxFrame - overhead
}

func effectiveMaximumReassembledInformationSize(limits CodecLimits) uint {
	if limits.MaximumReassembledInformationSize == 0 {
		return 65535
	}
	return limits.MaximumReassembledInformationSize
}
