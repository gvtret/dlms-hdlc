package hdlc_test

import (
	"testing"

	"dlms-hdlc/hdlc"
)

func expectRoundtrip(t *testing.T, value byte) {
	t.Helper()
	ctrl, err := hdlc.ControlFromByte(value)
	if err != nil {
		t.Fatalf("ControlFromByte(0x%02x): %v", value, err)
	}
	if ctrl.Encode() != value {
		t.Errorf("roundtrip(0x%02x): encoded to 0x%02x", value, ctrl.Encode())
	}
}

func TestDecodeIFrame(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x30)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindInformation {
		t.Errorf("frameKind: got %v, want Information", ctrl.FrameKind())
	}
	if !ctrl.PollFinal() {
		t.Error("PollFinal: want true")
	}
	if ctrl.SendSequence() != 0 {
		t.Errorf("N(S): got %d, want 0", ctrl.SendSequence())
	}
	if ctrl.ReceiveSequence() != 1 {
		t.Errorf("N(R): got %d, want 1", ctrl.ReceiveSequence())
	}
}

func TestDecodeIFrameWithoutPollFinal(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x42)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindInformation {
		t.Errorf("frameKind: got %v, want Information", ctrl.FrameKind())
	}
	if ctrl.PollFinal() {
		t.Error("PollFinal: want false")
	}
	if ctrl.SendSequence() != 1 {
		t.Errorf("N(S): got %d, want 1", ctrl.SendSequence())
	}
	if ctrl.ReceiveSequence() != 2 {
		t.Errorf("N(R): got %d, want 2", ctrl.ReceiveSequence())
	}
}

func TestDecodeReceiveReady(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x11)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindSupervisory {
		t.Errorf("frameKind: got %v, want Supervisory", ctrl.FrameKind())
	}
	kind, err2 := ctrl.SupervisoryKind()
	if err2 != nil {
		t.Fatal(err2)
	}
	if kind != hdlc.SupervisoryReceiveReady {
		t.Errorf("supervisory: got %v, want ReceiveReady", kind)
	}
	if !ctrl.PollFinal() {
		t.Error("PollFinal: want true")
	}
	if ctrl.ReceiveSequence() != 0 {
		t.Errorf("N(R): got %d, want 0", ctrl.ReceiveSequence())
	}
}

func TestDecodeReceiveNotReady(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x25)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindSupervisory {
		t.Errorf("frameKind: got %v, want Supervisory", ctrl.FrameKind())
	}
	if ctrl.PollFinal() {
		t.Error("PollFinal: want false")
	}
	if ctrl.ReceiveSequence() != 1 {
		t.Errorf("N(R): got %d, want 1", ctrl.ReceiveSequence())
	}
}

func TestDecodeReject(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x49)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindSupervisory {
		t.Errorf("frameKind: got %v, want Supervisory", ctrl.FrameKind())
	}
	if ctrl.ReceiveSequence() != 2 {
		t.Errorf("N(R): got %d, want 2", ctrl.ReceiveSequence())
	}
}

func TestDecodeSelectiveReject(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x6d)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindSupervisory {
		t.Errorf("frameKind: got %v, want Supervisory", ctrl.FrameKind())
	}
	if ctrl.ReceiveSequence() != 3 {
		t.Errorf("N(R): got %d, want 3", ctrl.ReceiveSequence())
	}
}

func TestDecodeSnrm(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x93)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindUnnumbered {
		t.Errorf("frameKind: got %v, want Unnumbered", ctrl.FrameKind())
	}
	kind, err2 := ctrl.UnnumberedKind()
	if err2 != nil {
		t.Fatal(err2)
	}
	if kind != hdlc.UnnumberedSnrm {
		t.Errorf("unnumbered: got %v, want Snrm", kind)
	}
	if !ctrl.PollFinal() {
		t.Error("PollFinal: want true")
	}
}

func TestDecodeUa(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x73)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindUnnumbered {
		t.Errorf("frameKind: got %v, want Unnumbered", ctrl.FrameKind())
	}
	if !ctrl.PollFinal() {
		t.Error("PollFinal: want true")
	}
}

func TestDecodeDisc(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x53)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindUnnumbered {
		t.Errorf("frameKind: got %v, want Unnumbered", ctrl.FrameKind())
	}
	if !ctrl.PollFinal() {
		t.Error("PollFinal: want true")
	}
}

func TestDecodeDm(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x1f)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindUnnumbered {
		t.Errorf("frameKind: got %v, want Unnumbered", ctrl.FrameKind())
	}
	if !ctrl.PollFinal() {
		t.Error("PollFinal: want true")
	}
}

func TestDecodeFrmr(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x97)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindUnnumbered {
		t.Errorf("frameKind: got %v, want Unnumbered", ctrl.FrameKind())
	}
	if !ctrl.PollFinal() {
		t.Error("PollFinal: want true")
	}
}

func TestDecodeUi(t *testing.T) {
	ctrl, err := hdlc.ControlFromByte(0x13)
	if err != nil {
		t.Fatal(err)
	}
	if ctrl.FrameKind() != hdlc.FrameKindUnnumbered {
		t.Errorf("frameKind: got %v, want Unnumbered", ctrl.FrameKind())
	}
	if !ctrl.PollFinal() {
		t.Error("PollFinal: want true")
	}
}

func TestDecodeRejectsUnsupportedUnnumbered(t *testing.T) {
	_, err := hdlc.ControlFromByte(0x33)
	if err != hdlc.StatusInvalidControlField {
		t.Errorf("expected InvalidControlField, got %v", err)
	}
}

func TestControlEncodeRoundtrip(t *testing.T) {
	for _, v := range []byte{0x30, 0x11, 0x25, 0x49, 0x6d, 0x93, 0x73, 0x53, 0x1f, 0x97, 0x13} {
		expectRoundtrip(t, v)
	}
}
