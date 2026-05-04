package hdlc_test

import (
	"testing"

	"dlms-hdlc/hdlc"
)

func makeSegBaseFrame(t *testing.T) hdlc.Frame {
	t.Helper()
	dst, err := hdlc.MakeServerAddress(0x01, 0x11)
	if err != nil {
		t.Fatal(err)
	}
	src, err := hdlc.MakeClientAddress(0x64)
	if err != nil {
		t.Fatal(err)
	}
	ctrl, err := hdlc.ControlFromByte(0x32)
	if err != nil {
		t.Fatal(err)
	}
	return hdlc.Frame{Destination: dst, Source: src, Control: ctrl}
}

func segOptions(maxInfo uint) hdlc.SegmentationOptions {
	limits := hdlc.DefaultCodecLimits()
	limits.MaximumInformationFieldSize = maxInfo
	return hdlc.SegmentationOptions{Limits: limits}
}

func TestSegmentInformationSingleFrame(t *testing.T) {
	base := makeSegBaseFrame(t)
	seg := hdlc.NewSegmenter(segOptions(4))
	frames, err := seg.Segment(base, []byte{0x01, 0x02, 0x03})
	if err != nil {
		t.Fatal(err)
	}
	if len(frames) != 1 {
		t.Fatalf("frames: got %d, want 1", len(frames))
	}
	if frames[0].Segmented {
		t.Error("segmented: want false")
	}
	if len(frames[0].Information) != 3 {
		t.Errorf("info size: got %d, want 3", len(frames[0].Information))
	}
}

func TestSegmentInformationMultipleFrames(t *testing.T) {
	base := makeSegBaseFrame(t)
	seg := hdlc.NewSegmenter(segOptions(2))
	frames, err := seg.Segment(base, []byte{0x01, 0x02, 0x03, 0x04, 0x05})
	if err != nil {
		t.Fatal(err)
	}
	if len(frames) != 3 {
		t.Fatalf("frames: got %d, want 3", len(frames))
	}
	if !frames[0].Segmented {
		t.Error("frame[0].segmented: want true")
	}
	if !frames[1].Segmented {
		t.Error("frame[1].segmented: want true")
	}
	if frames[2].Segmented {
		t.Error("frame[2].segmented: want false")
	}
	if len(frames[0].Information) != 2 {
		t.Errorf("frame[0] info size: got %d, want 2", len(frames[0].Information))
	}
	if len(frames[1].Information) != 2 {
		t.Errorf("frame[1] info size: got %d, want 2", len(frames[1].Information))
	}
	if len(frames[2].Information) != 1 {
		t.Errorf("frame[2] info size: got %d, want 1", len(frames[2].Information))
	}
}

func TestSegmentInformationExactBoundary(t *testing.T) {
	base := makeSegBaseFrame(t)
	seg := hdlc.NewSegmenter(segOptions(2))
	frames, err := seg.Segment(base, []byte{0x01, 0x02, 0x03, 0x04})
	if err != nil {
		t.Fatal(err)
	}
	if len(frames) != 2 {
		t.Fatalf("frames: got %d, want 2", len(frames))
	}
	if !frames[0].Segmented {
		t.Error("frame[0].segmented: want true")
	}
	if frames[1].Segmented {
		t.Error("frame[1].segmented: want false")
	}
}

func TestReassembleSingleFrame(t *testing.T) {
	base := makeSegBaseFrame(t)
	ra := hdlc.NewReassembler(hdlc.DefaultCodecLimits())

	frame := base
	frame.Segmented = false
	frame.Information = []byte{1, 2}

	completed, ok, err := ra.Push(frame)
	if err != nil {
		t.Fatal(err)
	}
	if !ok {
		t.Error("ok: want true")
	}
	if len(completed.Information) != 2 {
		t.Errorf("info size: got %d, want 2", len(completed.Information))
	}
}

func TestReassembleMultipleFrames(t *testing.T) {
	base := makeSegBaseFrame(t)
	ra := hdlc.NewReassembler(hdlc.DefaultCodecLimits())

	frame1 := base
	frame1.Segmented = true
	frame1.Information = []byte{1, 2}

	_, ok, err := ra.Push(frame1)
	if err != hdlc.StatusSegmentationIncomplete {
		t.Fatalf("frame1: expected SegmentationIncomplete, got %v", err)
	}
	if ok {
		t.Error("frame1 ok: want false")
	}

	frame2 := base
	frame2.Segmented = false
	frame2.Information = []byte{3}

	completed, ok, err := ra.Push(frame2)
	if err != nil {
		t.Fatalf("frame2: %v", err)
	}
	if !ok {
		t.Error("frame2 ok: want true")
	}
	if len(completed.Information) != 3 {
		t.Fatalf("reassembled size: got %d, want 3", len(completed.Information))
	}
	if completed.Information[0] != 1 || completed.Information[1] != 2 || completed.Information[2] != 3 {
		t.Errorf("reassembled payload: got %v, want [1 2 3]", completed.Information)
	}
	if completed.Segmented {
		t.Error("completed.segmented: want false")
	}
}

func TestReassembleAddressMismatch(t *testing.T) {
	base := makeSegBaseFrame(t)
	ra := hdlc.NewReassembler(hdlc.DefaultCodecLimits())

	frame1 := base
	frame1.Segmented = true
	frame1.Information = []byte{1}
	if _, _, err := ra.Push(frame1); err != hdlc.StatusSegmentationIncomplete {
		t.Fatalf("frame1: expected SegmentationIncomplete, got %v", err)
	}

	// Different server address (physical=0x12 instead of 0x11).
	wrongDst, err := hdlc.MakeServerAddress(0x01, 0x12)
	if err != nil {
		t.Fatal(err)
	}
	frame2 := base
	frame2.Destination = wrongDst
	frame2.Segmented = false
	frame2.Information = []byte{2}

	_, _, err = ra.Push(frame2)
	if err != hdlc.StatusSegmentationError {
		t.Errorf("expected SegmentationError, got %v", err)
	}
}

func TestReassembleControlMismatch(t *testing.T) {
	base := makeSegBaseFrame(t)
	ra := hdlc.NewReassembler(hdlc.DefaultCodecLimits())

	frame1 := base
	frame1.Segmented = true
	frame1.Information = []byte{1}
	if _, _, err := ra.Push(frame1); err != hdlc.StatusSegmentationIncomplete {
		t.Fatalf("frame1: expected SegmentationIncomplete, got %v", err)
	}

	// Different control byte (UI instead of I-frame).
	ctrl2, err := hdlc.ControlFromByte(0x13)
	if err != nil {
		t.Fatal(err)
	}
	frame2 := base
	frame2.Control = ctrl2
	frame2.Segmented = false
	frame2.Information = []byte{2}

	_, _, err = ra.Push(frame2)
	if err != hdlc.StatusSegmentationError {
		t.Errorf("expected SegmentationError, got %v", err)
	}
}

func TestReassembleOverflow(t *testing.T) {
	base := makeSegBaseFrame(t)
	limits := hdlc.DefaultCodecLimits()
	limits.MaximumReassembledInformationSize = 2
	ra := hdlc.NewReassembler(limits)

	frame1 := base
	frame1.Segmented = true
	frame1.Information = []byte{1, 2}
	if _, _, err := ra.Push(frame1); err != hdlc.StatusSegmentationIncomplete {
		t.Fatalf("frame1: expected SegmentationIncomplete, got %v", err)
	}

	frame2 := base
	frame2.Segmented = false
	frame2.Information = []byte{3}
	_, _, err := ra.Push(frame2)
	if err != hdlc.StatusSegmentationOverflow {
		t.Errorf("expected SegmentationOverflow, got %v", err)
	}
}

func TestReassembleNewSequenceBeforeCompletion(t *testing.T) {
	base := makeSegBaseFrame(t)
	ra := hdlc.NewReassembler(hdlc.DefaultCodecLimits())

	frame1 := base
	frame1.Segmented = true
	frame1.Information = []byte{1}
	if _, _, err := ra.Push(frame1); err != hdlc.StatusSegmentationIncomplete {
		t.Fatalf("frame1: expected SegmentationIncomplete, got %v", err)
	}

	wrongDst, err := hdlc.MakeServerAddress(0x01, 0x12)
	if err != nil {
		t.Fatal(err)
	}
	frame2 := base
	frame2.Destination = wrongDst
	frame2.Segmented = true
	frame2.Information = []byte{9}

	_, _, err = ra.Push(frame2)
	if err != hdlc.StatusSegmentationError {
		t.Errorf("expected SegmentationError, got %v", err)
	}
}
