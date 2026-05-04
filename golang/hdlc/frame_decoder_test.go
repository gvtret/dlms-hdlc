package hdlc_test

import (
	"testing"

	"dlms-hdlc/hdlc"
)

func TestDecodeValidSnrm(t *testing.T) {
	input := []byte{0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e}
	frame, err := hdlc.DecodeFrame(input, hdlc.DefaultCodecLimits())
	if err != nil {
		t.Fatal(err)
	}
	if frame.Segmented {
		t.Error("segmented: want false")
	}
	if frame.Destination.RawValue() != 0x91 {
		t.Errorf("dst raw: got 0x%x, want 0x91", frame.Destination.RawValue())
	}
	if frame.Destination.EncodedSize() != 2 {
		t.Errorf("dst size: got %d, want 2", frame.Destination.EncodedSize())
	}
	if frame.Source.RawValue() != 0x64 {
		t.Errorf("src raw: got 0x%x, want 0x64", frame.Source.RawValue())
	}
	if frame.Control.FrameKind() != hdlc.FrameKindUnnumbered {
		t.Errorf("frameKind: got %v, want Unnumbered", frame.Control.FrameKind())
	}
	if len(frame.Information) != 0 {
		t.Errorf("information: got %d bytes, want 0", len(frame.Information))
	}
}

func TestDecodeValidUaWithInformation(t *testing.T) {
	input := []byte{
		0x7e, 0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73, 0xb4,
		0x96, 0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06,
		0x01, 0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01,
		0x08, 0x04, 0x00, 0x00, 0x00, 0x01, 0x5f, 0x75,
		0x7e,
	}
	frame, err := hdlc.DecodeFrame(input, hdlc.DefaultCodecLimits())
	if err != nil {
		t.Fatal(err)
	}
	if frame.Segmented {
		t.Error("segmented: want false")
	}
	if frame.Destination.RawValue() != 0x64 {
		t.Errorf("dst raw: got 0x%x, want 0x64", frame.Destination.RawValue())
	}
	if frame.Source.RawValue() != 0x91 {
		t.Errorf("src raw: got 0x%x, want 0x91", frame.Source.RawValue())
	}
	if len(frame.Information) != 21 {
		t.Fatalf("information size: got %d, want 21", len(frame.Information))
	}
	if frame.Information[5] != 0x7e {
		t.Errorf("info[5]: got 0x%02x, want 0x7e", frame.Information[5])
	}
	if frame.Information[8] != 0x7e {
		t.Errorf("info[8]: got 0x%02x, want 0x7e", frame.Information[8])
	}
}

func TestDecodeValidIFrameWithPayloadFlagByte(t *testing.T) {
	input := []byte{
		0x7e, 0xa8, 0x10, 0x02, 0x23, 0xc9, 0x32, 0x5f,
		0x38, 0xe6, 0xe6, 0x00, 0x01, 0x7e, 0x02, 0xfe,
		0x1a, 0x7e,
	}
	frame, err := hdlc.DecodeFrame(input, hdlc.DefaultCodecLimits())
	if err != nil {
		t.Fatal(err)
	}
	if !frame.Segmented {
		t.Error("segmented: want true")
	}
	if frame.Control.FrameKind() != hdlc.FrameKindInformation {
		t.Errorf("frameKind: got %v, want Information", frame.Control.FrameKind())
	}
	if len(frame.Information) != 6 {
		t.Fatalf("information size: got %d, want 6", len(frame.Information))
	}
	if frame.Information[4] != 0x7e {
		t.Errorf("info[4]: got 0x%02x, want 0x7e", frame.Information[4])
	}
}

func TestDecodeFrameFromBufferCopiesInformation(t *testing.T) {
	input := []byte{
		0x7e, 0xa8, 0x10, 0x02, 0x23, 0xc9, 0x32, 0x5f,
		0x38, 0xe6, 0xe6, 0x00, 0x01, 0x7e, 0x02, 0xfe,
		0x1a, 0x7e,
	}
	infoBuf := make([]byte, 6)
	frame, infoSize, err := hdlc.DecodeFrameFromBuffer(input, hdlc.DefaultCodecLimits(), infoBuf)
	if err != nil {
		t.Fatal(err)
	}
	if infoSize != 6 {
		t.Errorf("infoSize: got %d, want 6", infoSize)
	}
	if frame.Information[4] != 0x7e {
		t.Errorf("info[4]: got 0x%02x, want 0x7e", frame.Information[4])
	}
}

func TestDecodeRejectsInvalidOpeningFlag(t *testing.T) {
	input := []byte{0x00, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e}
	_, err := hdlc.DecodeFrame(input, hdlc.DefaultCodecLimits())
	if err != hdlc.StatusInvalidFlag {
		t.Errorf("expected InvalidFlag, got %v", err)
	}
}

func TestDecodeRejectsInvalidFormatType(t *testing.T) {
	input := []byte{0x7e, 0xb0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e}
	_, err := hdlc.DecodeFrame(input, hdlc.DefaultCodecLimits())
	if err != hdlc.StatusInvalidFrameFormat {
		t.Errorf("expected InvalidFrameFormat, got %v", err)
	}
}

func TestDecodeRejectsInvalidLength(t *testing.T) {
	input := []byte{0x7e, 0xa0, 0x05, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e}
	_, err := hdlc.DecodeFrame(input, hdlc.DefaultCodecLimits())
	if err != hdlc.StatusInvalidFrameLength {
		t.Errorf("expected InvalidFrameLength, got %v", err)
	}
}

func TestDecodeReportsNeedMoreDataBeforeClosingFlag(t *testing.T) {
	input := []byte{0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43}
	_, err := hdlc.DecodeFrame(input, hdlc.DefaultCodecLimits())
	if err != hdlc.StatusNeedMoreData {
		t.Errorf("expected NeedMoreData, got %v", err)
	}
}

func TestDecodeRejectsWrongClosingFlag(t *testing.T) {
	input := []byte{0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x00}
	_, err := hdlc.DecodeFrame(input, hdlc.DefaultCodecLimits())
	if err != hdlc.StatusInvalidFlag {
		t.Errorf("expected InvalidFlag, got %v", err)
	}
}

func TestDecodeRejectsInvalidHeaderChecksum(t *testing.T) {
	input := []byte{
		0x7e, 0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73, 0xb4,
		0x96, 0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06,
		0x01, 0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01,
		0x08, 0x04, 0x00, 0x00, 0x00, 0x01, 0x5f, 0x75,
		0x7e,
	}
	input[7] ^= 0x01
	_, err := hdlc.DecodeFrame(input, hdlc.DefaultCodecLimits())
	if err != hdlc.StatusInvalidHeaderChecksum {
		t.Errorf("expected InvalidHeaderChecksum, got %v", err)
	}
}

func TestDecodeRejectsInvalidFrameChecksum(t *testing.T) {
	input := []byte{0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e}
	input[7] ^= 0x01
	_, err := hdlc.DecodeFrame(input, hdlc.DefaultCodecLimits())
	if err != hdlc.StatusInvalidFrameChecksum {
		t.Errorf("expected InvalidFrameChecksum, got %v", err)
	}
}

func TestDecodeRejectsSmallInformationBuffer(t *testing.T) {
	input := []byte{
		0x7e, 0xa8, 0x10, 0x02, 0x23, 0xc9, 0x32, 0x5f,
		0x38, 0xe6, 0xe6, 0x00, 0x01, 0x7e, 0x02, 0xfe,
		0x1a, 0x7e,
	}
	infoBuf := make([]byte, 4)
	_, _, err := hdlc.DecodeFrameFromBuffer(input, hdlc.DefaultCodecLimits(), infoBuf)
	if err != hdlc.StatusOutputBufferTooSmall {
		t.Errorf("expected OutputBufferTooSmall, got %v", err)
	}
}
