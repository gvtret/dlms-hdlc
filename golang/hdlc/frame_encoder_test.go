package hdlc_test

import (
	"bytes"
	"testing"

	"dlms-hdlc/hdlc"
)

// makeTestFrame builds a frame with server destination (logical=0x01, physical=0x11)
// and client source (0x64) — the canonical test addresses used across the C++ test suite.
func makeTestFrame(t *testing.T, controlValue byte) hdlc.Frame {
	t.Helper()
	dst, err := hdlc.MakeServerAddress(0x01, 0x11)
	if err != nil {
		t.Fatal(err)
	}
	src, err := hdlc.MakeClientAddress(0x64)
	if err != nil {
		t.Fatal(err)
	}
	ctrl, err := hdlc.ControlFromByte(controlValue)
	if err != nil {
		t.Fatal(err)
	}
	return hdlc.Frame{Destination: dst, Source: src, Control: ctrl}
}

func encodeOrFail(t *testing.T, frame hdlc.Frame) []byte {
	t.Helper()
	out, err := hdlc.EncodeFrame(frame, hdlc.DefaultCodecLimits())
	if err != nil {
		t.Fatalf("EncodeFrame: %v", err)
	}
	return out
}

func TestEncodeSnrm(t *testing.T) {
	frame := makeTestFrame(t, 0x93)
	expected := []byte{0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e}
	if out := encodeOrFail(t, frame); !bytes.Equal(out, expected) {
		t.Errorf("encoded: got %x\nwant:      %x", out, expected)
	}
}

func TestEncodeDisc(t *testing.T) {
	frame := makeTestFrame(t, 0x53)
	expected := []byte{0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x53, 0xe8, 0x85, 0x7e}
	if out := encodeOrFail(t, frame); !bytes.Equal(out, expected) {
		t.Errorf("encoded: got %x\nwant:      %x", out, expected)
	}
}

func TestEncodeReceiveReady(t *testing.T) {
	frame := makeTestFrame(t, 0x11)
	expected := []byte{0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x11, 0xfe, 0xe4, 0x7e}
	if out := encodeOrFail(t, frame); !bytes.Equal(out, expected) {
		t.Errorf("encoded: got %x\nwant:      %x", out, expected)
	}
}

func TestEncodeUaWithInformation(t *testing.T) {
	// UA response reverses addresses: client is destination, server is source.
	dst, err := hdlc.MakeClientAddress(0x64)
	if err != nil {
		t.Fatal(err)
	}
	src, err := hdlc.MakeServerAddress(0x01, 0x11)
	if err != nil {
		t.Fatal(err)
	}
	ctrl, err := hdlc.ControlFromByte(0x73)
	if err != nil {
		t.Fatal(err)
	}
	information := []byte{
		0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06, 0x01,
		0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01, 0x08,
		0x04, 0x00, 0x00, 0x00, 0x01,
	}
	frame := hdlc.Frame{Destination: dst, Source: src, Control: ctrl, Information: information}
	expected := []byte{
		0x7e, 0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73, 0xb4,
		0x96, 0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06,
		0x01, 0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01,
		0x08, 0x04, 0x00, 0x00, 0x00, 0x01, 0x5f, 0x75,
		0x7e,
	}
	if out := encodeOrFail(t, frame); !bytes.Equal(out, expected) {
		t.Errorf("encoded: got %x\nwant:      %x", out, expected)
	}
}

func TestEncodeIFrameWithSegmentationBitAndPayloadFlagByte(t *testing.T) {
	frame := makeTestFrame(t, 0x32)
	frame.Segmented = true
	frame.Information = []byte{0xe6, 0xe6, 0x00, 0x01, 0x7e, 0x02}

	out := encodeOrFail(t, frame)

	if len(out) != 18 {
		t.Fatalf("size: got %d, want 18", len(out))
	}
	if out[0] != 0x7e {
		t.Errorf("out[0]: got 0x%02x, want 0x7e", out[0])
	}
	if out[1] != 0xa8 {
		t.Errorf("out[1]: got 0x%02x, want 0xa8", out[1])
	}
	if out[2] != 0x10 {
		t.Errorf("out[2]: got 0x%02x, want 0x10", out[2])
	}
	if out[13] != 0x7e {
		t.Errorf("out[13]: got 0x%02x, want 0x7e (payload flag byte)", out[13])
	}
	if out[17] != 0x7e {
		t.Errorf("out[17]: got 0x%02x, want 0x7e (closing flag)", out[17])
	}
}

func TestEncodeFrameToBufferReportsSmallOutput(t *testing.T) {
	frame := makeTestFrame(t, 0x93)
	buf := make([]byte, 4)
	n, err := hdlc.EncodeFrameToBuffer(frame, hdlc.DefaultCodecLimits(), buf)
	if err != hdlc.StatusOutputBufferTooSmall {
		t.Errorf("expected OutputBufferTooSmall, got %v", err)
	}
	if n != 0 {
		t.Errorf("written: got %d, want 0", n)
	}
}

func TestEncodeRejectsInformationFieldTooLarge(t *testing.T) {
	frame := makeTestFrame(t, 0x32)
	frame.Information = []byte{0x01, 0x02}
	limits := hdlc.DefaultCodecLimits()
	limits.MaximumInformationFieldSize = 1
	buf := make([]byte, 32)
	_, err := hdlc.EncodeFrameToBuffer(frame, limits, buf)
	if err != hdlc.StatusInformationFieldTooLarge {
		t.Errorf("expected InformationFieldTooLarge, got %v", err)
	}
}

func TestEncodeRejectsFrameTooLarge(t *testing.T) {
	frame := makeTestFrame(t, 0x93)
	limits := hdlc.DefaultCodecLimits()
	limits.MaximumFrameSize = 9
	buf := make([]byte, 32)
	_, err := hdlc.EncodeFrameToBuffer(frame, limits, buf)
	if err != hdlc.StatusFrameTooLarge {
		t.Errorf("expected FrameTooLarge, got %v", err)
	}
}
