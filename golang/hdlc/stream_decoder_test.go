package hdlc_test

import (
	"testing"

	"dlms-hdlc/hdlc"
)

var (
	kSnrmFrameSD = []byte{0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e}
	kRrFrameSD   = []byte{0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x11, 0xfe, 0xe4, 0x7e}
	kIFrameWithPayloadFlagSD = []byte{
		0x7e, 0xa8, 0x10, 0x02, 0x23, 0xc9, 0x32, 0x5f,
		0x38, 0xe6, 0xe6, 0x00, 0x01, 0x7e, 0x02, 0xfe,
		0x1a, 0x7e,
	}
)

func makeDecoderOptions(policy hdlc.NoisePolicy) hdlc.StreamDecoderOptions {
	return hdlc.StreamDecoderOptions{
		Limits:      hdlc.DefaultCodecLimits(),
		NoisePolicy: policy,
	}
}

func TestStreamDecoderPushFullFrame(t *testing.T) {
	dec := hdlc.NewStreamDecoder(makeDecoderOptions(hdlc.NoisePolicyIgnore))
	frames, err := dec.Push(kSnrmFrameSD)
	if err != nil && err != hdlc.StatusNeedMoreData {
		t.Fatalf("Push: %v", err)
	}
	if len(frames) != 1 {
		t.Fatalf("frames: got %d, want 1", len(frames))
	}
	if frames[0].Destination.RawValue() != 0x91 {
		t.Errorf("dst raw: got 0x%x, want 0x91", frames[0].Destination.RawValue())
	}
}

func TestStreamDecoderPushByteByByte(t *testing.T) {
	dec := hdlc.NewStreamDecoder(makeDecoderOptions(hdlc.NoisePolicyIgnore))

	for i := 0; i < len(kSnrmFrameSD)-1; i++ {
		frames, err := dec.Push(kSnrmFrameSD[i : i+1])
		if err != hdlc.StatusNeedMoreData {
			t.Fatalf("byte %d: expected NeedMoreData, got %v", i, err)
		}
		if len(frames) != 0 {
			t.Fatalf("byte %d: got %d frames, want 0", i, len(frames))
		}
	}

	frames, err := dec.Push(kSnrmFrameSD[len(kSnrmFrameSD)-1:])
	if err != nil && err != hdlc.StatusNeedMoreData {
		t.Fatalf("last byte: %v", err)
	}
	if len(frames) != 1 {
		t.Fatalf("frames: got %d, want 1", len(frames))
	}
}

func TestStreamDecoderPushMultipleFrames(t *testing.T) {
	input := append(append([]byte{}, kSnrmFrameSD...), kRrFrameSD...)
	dec := hdlc.NewStreamDecoder(makeDecoderOptions(hdlc.NoisePolicyIgnore))
	frames, err := dec.Push(input)
	if err != nil && err != hdlc.StatusNeedMoreData {
		t.Fatalf("Push: %v", err)
	}
	if len(frames) != 2 {
		t.Fatalf("frames: got %d, want 2", len(frames))
	}
}

func TestStreamDecoderPushFrameWithPayloadFlagByte(t *testing.T) {
	dec := hdlc.NewStreamDecoder(makeDecoderOptions(hdlc.NoisePolicyIgnore))
	frames, err := dec.Push(kIFrameWithPayloadFlagSD)
	if err != nil && err != hdlc.StatusNeedMoreData {
		t.Fatalf("Push: %v", err)
	}
	if len(frames) != 1 {
		t.Fatalf("frames: got %d, want 1", len(frames))
	}
	if len(frames[0].Information) != 6 {
		t.Fatalf("info size: got %d, want 6", len(frames[0].Information))
	}
	if frames[0].Information[4] != 0x7e {
		t.Errorf("info[4]: got 0x%02x, want 0x7e", frames[0].Information[4])
	}
}

func TestStreamDecoderPushNoiseBeforeFlagIgnorePolicy(t *testing.T) {
	input := append([]byte{0x00, 0xff}, kSnrmFrameSD...)
	dec := hdlc.NewStreamDecoder(makeDecoderOptions(hdlc.NoisePolicyIgnore))
	frames, err := dec.Push(input)
	if err != nil && err != hdlc.StatusNeedMoreData {
		t.Fatalf("Push: %v", err)
	}
	if len(frames) != 1 {
		t.Fatalf("frames: got %d, want 1", len(frames))
	}
}

func TestStreamDecoderPushNoiseBeforeFlagErrorPolicy(t *testing.T) {
	dec := hdlc.NewStreamDecoder(makeDecoderOptions(hdlc.NoisePolicyReportError))
	_, err := dec.Push([]byte{0x00, 0xff})
	if err != hdlc.StatusInvalidFlag {
		t.Errorf("expected InvalidFlag, got %v", err)
	}
}

func TestStreamDecoderPushInvalidLength(t *testing.T) {
	input := []byte{0x7e, 0xa0, 0x05, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e}
	dec := hdlc.NewStreamDecoder(makeDecoderOptions(hdlc.NoisePolicyIgnore))
	_, err := dec.Push(input)
	if err != hdlc.StatusInvalidFrameLength {
		t.Errorf("expected InvalidFrameLength, got %v", err)
	}
}

func TestStreamDecoderPushFrameTooLarge(t *testing.T) {
	opts := makeDecoderOptions(hdlc.NoisePolicyIgnore)
	opts.Limits.MaximumFrameSize = 9
	dec := hdlc.NewStreamDecoder(opts)
	_, err := dec.Push(kSnrmFrameSD)
	if err != hdlc.StatusFrameTooLarge {
		t.Errorf("expected FrameTooLarge, got %v", err)
	}
}

func TestStreamDecoderPushMissingClosingFlag(t *testing.T) {
	dec := hdlc.NewStreamDecoder(makeDecoderOptions(hdlc.NoisePolicyIgnore))
	frames, err := dec.Push(kSnrmFrameSD[:len(kSnrmFrameSD)-1])
	if err != hdlc.StatusNeedMoreData {
		t.Errorf("expected NeedMoreData, got %v", err)
	}
	if len(frames) != 0 {
		t.Errorf("frames: got %d, want 0", len(frames))
	}
}

func TestStreamDecoderPushWrongClosingFlag(t *testing.T) {
	input := []byte{0x7e, 0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x00}
	dec := hdlc.NewStreamDecoder(makeDecoderOptions(hdlc.NoisePolicyIgnore))
	_, err := dec.Push(input)
	if err != hdlc.StatusInvalidFlag {
		t.Errorf("expected InvalidFlag, got %v", err)
	}
}

func TestStreamDecoderPushResetAfterError(t *testing.T) {
	invalid := []byte{0x7e, 0xa0, 0x05, 0x02, 0x23, 0xc9, 0x93, 0xe4, 0x43, 0x7e}
	dec := hdlc.NewStreamDecoder(makeDecoderOptions(hdlc.NoisePolicyIgnore))

	if _, err := dec.Push(invalid); err != hdlc.StatusInvalidFrameLength {
		t.Fatalf("expected InvalidFrameLength, got %v", err)
	}

	dec.Reset()

	frames, err := dec.Push(kRrFrameSD)
	if err != nil && err != hdlc.StatusNeedMoreData {
		t.Fatalf("after reset Push: %v", err)
	}
	if len(frames) != 1 {
		t.Fatalf("frames after reset: got %d, want 1", len(frames))
	}
}
