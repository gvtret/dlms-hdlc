package hdlc

import "encoding/binary"

const (
	hdlcFlag             = byte(0x7e)
	frameFormatType3     = uint16(0xa000)
	frameFormatTypeMask  = uint16(0xf000)
	segmentationBit      = uint16(0x0800)
	maxFormatFieldLength = uint(2047)
)

// EncodeFrameToBuffer encodes one complete HDLC Type 3 frame into the
// caller-provided output slice. The encoder writes the opening flag, Frame
// Format field (big-endian), destination and source addresses, control field,
// optional HCS, opaque Information bytes, FCS (both checksums little-endian),
// and closing flag. No dynamic allocation is performed.
// Returns the number of bytes written, or an error:
// StatusOutputBufferTooSmall, StatusFrameTooLarge, StatusInformationFieldTooLarge.
func EncodeFrameToBuffer(frame Frame, limits CodecLimits, output []byte) (int, error) {
	destEnc := frame.Destination.Encode()
	srcEnc := frame.Source.Encode()

	destSize := frame.Destination.EncodedSize()
	srcSize := frame.Source.EncodedSize()

	maxInfo := effectiveMaximumInformationFieldSize(destSize, srcSize, limits)
	if uint(len(frame.Information)) > maxInfo {
		return 0, StatusInformationFieldTooLarge
	}

	hasInfo := len(frame.Information) > 0
	headerSize := 2 + len(destEnc) + len(srcEnc) + 1
	hcsSize := 0
	if hasInfo {
		hcsSize = 2
	}
	formatFieldLength := headerSize + hcsSize + len(frame.Information) + 2
	fullFrameSize := formatFieldLength + 2

	if uint(formatFieldLength) > maxFormatFieldLength {
		return 0, StatusInvalidFrameLength
	}
	if uint(fullFrameSize) > effectiveMaximumFrameSize(limits) {
		return 0, StatusFrameTooLarge
	}
	if len(output) < fullFrameSize {
		return 0, StatusOutputBufferTooSmall
	}

	offset := 0

	output[offset] = hdlcFlag
	offset++

	var seg uint16
	if frame.Segmented {
		seg = segmentationBit
	}
	binary.BigEndian.PutUint16(output[offset:], frameFormatType3|seg|uint16(formatFieldLength))
	offset += 2

	copy(output[offset:], destEnc)
	offset += len(destEnc)

	copy(output[offset:], srcEnc)
	offset += len(srcEnc)

	output[offset] = frame.Control.Encode()
	offset++

	if hasInfo {
		// HCS covers format field + addresses + control (bytes from index 1 to offset).
		hcs := calculateHdlcCRC(output[1:offset])
		binary.LittleEndian.PutUint16(output[offset:], hcs)
		offset += 2

		copy(output[offset:], frame.Information)
		offset += len(frame.Information)
	}

	// FCS covers everything from format field to just before FCS (bytes 1..offset).
	fcs := calculateHdlcCRC(output[1:offset])
	binary.LittleEndian.PutUint16(output[offset:], fcs)
	offset += 2

	output[offset] = hdlcFlag
	offset++

	return offset, nil
}

// EncodeFrame encodes one complete HDLC Type 3 frame and returns the encoded
// bytes as a new slice. This convenience wrapper allocates internally;
// use EncodeFrameToBuffer when allocation must be avoided.
func EncodeFrame(frame Frame, limits CodecLimits) ([]byte, error) {
	buf := make([]byte, effectiveMaximumFrameSize(limits))
	n, err := EncodeFrameToBuffer(frame, limits, buf)
	if err != nil {
		return nil, err
	}
	return buf[:n], nil
}

// DecodeFrameFromBuffer decodes one complete HDLC Type 3 frame from input.
// The decoder validates opening and closing flags, frame format type, length,
// addresses, control field, optional HCS, FCS, and configured limits.
// infoBuf is caller-provided storage for the decoded Information field;
// pass nil to let the function allocate a new slice.
// Returns the decoded Frame, the Information field byte count, and an error.
// Returns StatusOutputBufferTooSmall when infoBuf is provided but too small.
func DecodeFrameFromBuffer(input []byte, limits CodecLimits, infoBuf []byte) (Frame, int, error) {
	if len(input) < 4 {
		return Frame{}, 0, StatusNeedMoreData
	}
	if input[0] != hdlcFlag {
		return Frame{}, 0, StatusInvalidFlag
	}

	ff := binary.BigEndian.Uint16(input[1:])
	if ff&frameFormatTypeMask != frameFormatType3 {
		return Frame{}, 0, StatusInvalidFrameFormat
	}

	segmented := ff&segmentationBit != 0
	formatFieldLength := int(ff & 0x07ff)
	fullFrameSize := formatFieldLength + 2

	if formatFieldLength < 6 {
		return Frame{}, 0, StatusInvalidFrameLength
	}
	if uint(formatFieldLength) > maxFormatFieldLength {
		return Frame{}, 0, StatusInvalidFrameLength
	}
	if uint(fullFrameSize) > effectiveMaximumFrameSize(limits) {
		return Frame{}, 0, StatusFrameTooLarge
	}
	if len(input) < fullFrameSize {
		return Frame{}, 0, StatusNeedMoreData
	}
	if len(input) != fullFrameSize {
		return Frame{}, 0, StatusInvalidFrameLength
	}
	if input[fullFrameSize-1] != hdlcFlag {
		return Frame{}, 0, StatusInvalidFlag
	}

	offset := 3

	dst, consumed, err := AddressFromBytes(input[offset : fullFrameSize-1])
	if err != nil {
		return Frame{}, 0, err
	}
	offset += consumed

	src, consumed, err := AddressFromBytes(input[offset : fullFrameSize-1])
	if err != nil {
		return Frame{}, 0, err
	}
	offset += consumed

	if offset >= fullFrameSize-3 {
		return Frame{}, 0, StatusInvalidFrameLength
	}

	ctrl, err := ControlFromByte(input[offset])
	if err != nil {
		return Frame{}, 0, err
	}
	// headerSize = bytes from index 1 up to and including control byte.
	headerSize := offset // = 3 + destConsumed + srcConsumed (equals offset before ctrl byte)
	offset++

	bytesBeforeFCS := fullFrameSize - 3 - offset
	if bytesBeforeFCS == 1 {
		return Frame{}, 0, StatusInvalidFrameLength
	}

	hasInfo := bytesBeforeFCS != 0
	if hasInfo {
		expectedHCS := binary.LittleEndian.Uint16(input[offset:])
		// HCS covers input[1 : 1+headerSize] = format + dest + src + ctrl.
		if calculateHdlcCRC(input[1:1+headerSize]) != expectedHCS {
			return Frame{}, 0, StatusInvalidHeaderChecksum
		}
		offset += 2
	}

	// FCS covers input[1 : fullFrameSize-3] = everything before FCS bytes.
	expectedFCS := binary.LittleEndian.Uint16(input[fullFrameSize-3:])
	if calculateHdlcCRC(input[1:fullFrameSize-3]) != expectedFCS {
		return Frame{}, 0, StatusInvalidFrameChecksum
	}

	infoSize := 0
	if hasInfo {
		infoSize = fullFrameSize - 3 - offset
	}

	maxInfo := effectiveMaximumInformationFieldSize(dst.EncodedSize(), src.EncodedSize(), limits)
	if uint(infoSize) > maxInfo {
		return Frame{}, 0, StatusInformationFieldTooLarge
	}

	var info []byte
	if infoSize > 0 {
		if infoBuf != nil {
			if infoSize > len(infoBuf) {
				return Frame{}, 0, StatusOutputBufferTooSmall
			}
			copy(infoBuf, input[offset:offset+infoSize])
			info = infoBuf[:infoSize]
		} else {
			info = make([]byte, infoSize)
			copy(info, input[offset:offset+infoSize])
		}
	}

	return Frame{
		Segmented:   segmented,
		Destination: dst,
		Source:      src,
		Control:     ctrl,
		Information: info,
	}, infoSize, nil
}

// DecodeFrame decodes one complete HDLC Type 3 frame and returns it with an
// owned Information slice. This convenience wrapper allocates for the
// Information field; use DecodeFrameFromBuffer when allocation must be avoided.
func DecodeFrame(input []byte, limits CodecLimits) (Frame, error) {
	frame, _, err := DecodeFrameFromBuffer(input, limits, nil)
	return frame, err
}
