// Package main is a CGo-based C ABI wrapper for the dlms-hdlc Go implementation.
// Build as a shared library:
//
//	go build -buildmode=c-shared -o libdlms_hdlc.so ./cabi
//
// Build as a static archive:
//
//	go build -buildmode=c-archive -o libdlms_hdlc.a ./cabi
//
// The exported functions match hdlc_c_api.h exactly.
package main

/*
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// Status codes — must match hdlc_c_api.h exactly.
typedef int dlms_hdlc_status_t;
#define DLMS_HDLC_STATUS_OK                        0
#define DLMS_HDLC_STATUS_NEED_MORE_DATA            1
#define DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL   2
#define DLMS_HDLC_STATUS_INVALID_ARGUMENT          3
#define DLMS_HDLC_STATUS_INVALID_FLAG              4
#define DLMS_HDLC_STATUS_INVALID_FRAME_FORMAT      5
#define DLMS_HDLC_STATUS_INVALID_FRAME_TYPE        6
#define DLMS_HDLC_STATUS_INVALID_FRAME_LENGTH      7
#define DLMS_HDLC_STATUS_INVALID_ADDRESS           8
#define DLMS_HDLC_STATUS_INVALID_CONTROL_FIELD     9
#define DLMS_HDLC_STATUS_INVALID_HEADER_CHECKSUM   10
#define DLMS_HDLC_STATUS_INVALID_FRAME_CHECKSUM    11
#define DLMS_HDLC_STATUS_FRAME_TOO_LARGE           12
#define DLMS_HDLC_STATUS_INFORMATION_FIELD_TOO_LARGE 13
#define DLMS_HDLC_STATUS_SEGMENTATION_ERROR        14
#define DLMS_HDLC_STATUS_SEGMENTATION_INCOMPLETE   15
#define DLMS_HDLC_STATUS_SEGMENTATION_OVERFLOW     16
#define DLMS_HDLC_STATUS_UNSUPPORTED_FRAME         17
#define DLMS_HDLC_STATUS_UNSUPPORTED_ADDRESS       18
#define DLMS_HDLC_STATUS_UNSUPPORTED_FEATURE       19
#define DLMS_HDLC_STATUS_INTERNAL_ERROR            20

typedef struct dlms_hdlc_limits_t {
  size_t maximum_frame_size;
  size_t maximum_information_field_size;
  size_t maximum_reassembled_information_size;
} dlms_hdlc_limits_t;

typedef struct dlms_hdlc_frame_t {
  uint8_t  segmented;
  uint32_t destination_address_raw;
  size_t   destination_address_size;
  uint32_t source_address_raw;
  size_t   source_address_size;
  uint8_t  control;
  const uint8_t* information_data;
  size_t   information_size;
} dlms_hdlc_frame_t;

typedef struct dlms_hdlc_stream_decoder_t {
  uintptr_t _handle;
} dlms_hdlc_stream_decoder_t;

typedef struct dlms_hdlc_reassembler_t {
  uintptr_t _handle;
} dlms_hdlc_reassembler_t;
*/
import "C"
import (
	"sync"
	"unsafe"

	"dlms-hdlc/hdlc"
)

// ---------------------------------------------------------------------------
// Handle registry — maps uintptr IDs to Go objects so no Go pointer is stored
// in C-allocated memory (required by CGo pointer rules).
// ---------------------------------------------------------------------------

var (
	mu             sync.Mutex
	nextHandleID   uintptr = 1
	streamHandles          = make(map[uintptr]*streamDecoderState)
	reassemblerMap         = make(map[uintptr]*hdlc.Reassembler)
)

type streamDecoderState struct {
	decoder       *hdlc.StreamDecoder
	pendingFrames []hdlc.Frame
}

func newStreamHandle(state *streamDecoderState) uintptr {
	mu.Lock()
	id := nextHandleID
	nextHandleID++
	streamHandles[id] = state
	mu.Unlock()
	return id
}

func getStreamHandle(id uintptr) *streamDecoderState {
	mu.Lock()
	s := streamHandles[id]
	mu.Unlock()
	return s
}

func deleteStreamHandle(id uintptr) {
	mu.Lock()
	delete(streamHandles, id)
	mu.Unlock()
}

func newReassemblerHandle(r *hdlc.Reassembler) uintptr {
	mu.Lock()
	id := nextHandleID
	nextHandleID++
	reassemblerMap[id] = r
	mu.Unlock()
	return id
}

func getReassemblerHandle(id uintptr) *hdlc.Reassembler {
	mu.Lock()
	r := reassemblerMap[id]
	mu.Unlock()
	return r
}

func deleteReassemblerHandle(id uintptr) {
	mu.Lock()
	delete(reassemblerMap, id)
	mu.Unlock()
}

// ---------------------------------------------------------------------------
// Conversion helpers
// ---------------------------------------------------------------------------

func toLimits(p *C.dlms_hdlc_limits_t) hdlc.CodecLimits {
	limits := hdlc.DefaultCodecLimits()
	if p != nil {
		limits.MaximumFrameSize = uint(p.maximum_frame_size)
		limits.MaximumInformationFieldSize = uint(p.maximum_information_field_size)
		limits.MaximumReassembledInformationSize = uint(p.maximum_reassembled_information_size)
	}
	return limits
}

func toStatus(err error) C.dlms_hdlc_status_t {
	if err == nil {
		return C.DLMS_HDLC_STATUS_OK
	}
	if s, ok := err.(hdlc.Status); ok {
		return C.dlms_hdlc_status_t(s)
	}
	return C.DLMS_HDLC_STATUS_INTERNAL_ERROR
}

func goFrame(cf *C.dlms_hdlc_frame_t) (hdlc.Frame, error) {
	if cf.information_data == nil && cf.information_size != 0 {
		return hdlc.Frame{}, hdlc.StatusInvalidArgument
	}

	dst, err := hdlc.AddressFromRaw(uint32(cf.destination_address_raw), uint(cf.destination_address_size))
	if err != nil {
		return hdlc.Frame{}, err
	}
	src, err := hdlc.AddressFromRaw(uint32(cf.source_address_raw), uint(cf.source_address_size))
	if err != nil {
		return hdlc.Frame{}, err
	}
	ctrl, err := hdlc.ControlFromByte(byte(cf.control))
	if err != nil {
		return hdlc.Frame{}, err
	}

	var info []byte
	if cf.information_size > 0 {
		info = C.GoBytes(unsafe.Pointer(cf.information_data), C.int(cf.information_size))
	}

	return hdlc.Frame{
		Segmented:   cf.segmented != 0,
		Destination: dst,
		Source:      src,
		Control:     ctrl,
		Information: info,
	}, nil
}

func fillCFrame(frame hdlc.Frame, infoBuf *C.uint8_t, infoSize C.size_t, cf *C.dlms_hdlc_frame_t) C.dlms_hdlc_status_t {
	if cf.segmented != 0 {
		cf.segmented = 1
	} else {
		cf.segmented = 0
	}
	cf.destination_address_raw = C.uint32_t(frame.Destination.RawValue())
	cf.destination_address_size = C.size_t(frame.Destination.EncodedSize())
	cf.source_address_raw = C.uint32_t(frame.Source.RawValue())
	cf.source_address_size = C.size_t(frame.Source.EncodedSize())
	cf.control = C.uint8_t(frame.Control.Encode())
	cf.information_data = infoBuf
	cf.information_size = infoSize
	if frame.Segmented {
		cf.segmented = 1
	} else {
		cf.segmented = 0
	}
	return C.DLMS_HDLC_STATUS_OK
}

// ---------------------------------------------------------------------------
// Encode / Decode
// ---------------------------------------------------------------------------

//export dlms_hdlc_encode_frame
func dlms_hdlc_encode_frame(
	cframe *C.dlms_hdlc_frame_t,
	climits *C.dlms_hdlc_limits_t,
	output *C.uint8_t,
	outputSize C.size_t,
	writtenSize *C.size_t,
) C.dlms_hdlc_status_t {
	if writtenSize != nil {
		*writtenSize = 0
	}
	if cframe == nil || output == nil || writtenSize == nil {
		return C.DLMS_HDLC_STATUS_INVALID_ARGUMENT
	}

	frame, err := goFrame(cframe)
	if err != nil {
		return toStatus(err)
	}

	limits := toLimits(climits)
	buf := unsafe.Slice((*byte)(unsafe.Pointer(output)), int(outputSize))
	n, err := hdlc.EncodeFrameToBuffer(frame, limits, buf)
	if err != nil {
		return toStatus(err)
	}
	*writtenSize = C.size_t(n)
	return C.DLMS_HDLC_STATUS_OK
}

//export dlms_hdlc_decode_frame
func dlms_hdlc_decode_frame(
	input *C.uint8_t,
	inputSize C.size_t,
	climits *C.dlms_hdlc_limits_t,
	cframe *C.dlms_hdlc_frame_t,
	infoBuf *C.uint8_t,
	infoBufSize C.size_t,
	infoSize *C.size_t,
) C.dlms_hdlc_status_t {
	if infoSize != nil {
		*infoSize = 0
	}
	if input == nil || cframe == nil || infoSize == nil {
		return C.DLMS_HDLC_STATUS_INVALID_ARGUMENT
	}

	limits := toLimits(climits)
	inputSlice := unsafe.Slice((*byte)(unsafe.Pointer(input)), int(inputSize))

	var ibuf []byte
	if infoBuf != nil && infoBufSize > 0 {
		ibuf = unsafe.Slice((*byte)(unsafe.Pointer(infoBuf)), int(infoBufSize))
	}

	frame, written, err := hdlc.DecodeFrameFromBuffer(inputSlice, limits, ibuf)
	if err != nil {
		return toStatus(err)
	}

	*infoSize = C.size_t(written)
	return fillCFrame(frame, infoBuf, C.size_t(written), cframe)
}

// ---------------------------------------------------------------------------
// Stream decoder lifecycle
// ---------------------------------------------------------------------------

//export dlms_hdlc_stream_decoder_create
func dlms_hdlc_stream_decoder_create(
	climits *C.dlms_hdlc_limits_t,
	decoderOut **C.dlms_hdlc_stream_decoder_t,
) C.dlms_hdlc_status_t {
	if decoderOut == nil {
		return C.DLMS_HDLC_STATUS_INVALID_ARGUMENT
	}
	*decoderOut = nil

	opts := hdlc.StreamDecoderOptions{
		Limits:      toLimits(climits),
		NoisePolicy: hdlc.NoisePolicyIgnore,
	}
	state := &streamDecoderState{decoder: hdlc.NewStreamDecoder(opts)}
	id := newStreamHandle(state)

	p := (*C.dlms_hdlc_stream_decoder_t)(C.malloc(C.sizeof_dlms_hdlc_stream_decoder_t))
	if p == nil {
		deleteStreamHandle(id)
		return C.DLMS_HDLC_STATUS_INTERNAL_ERROR
	}
	p._handle = C.uintptr_t(id)
	*decoderOut = p
	return C.DLMS_HDLC_STATUS_OK
}

//export dlms_hdlc_stream_decoder_destroy
func dlms_hdlc_stream_decoder_destroy(decoder *C.dlms_hdlc_stream_decoder_t) {
	if decoder == nil {
		return
	}
	deleteStreamHandle(uintptr(decoder._handle))
	C.free(unsafe.Pointer(decoder))
}

//export dlms_hdlc_stream_decoder_reset
func dlms_hdlc_stream_decoder_reset(decoder *C.dlms_hdlc_stream_decoder_t) {
	if decoder == nil {
		return
	}
	state := getStreamHandle(uintptr(decoder._handle))
	if state == nil {
		return
	}
	state.decoder.Reset()
	state.pendingFrames = nil
}

//export dlms_hdlc_stream_decoder_push
func dlms_hdlc_stream_decoder_push(
	decoder *C.dlms_hdlc_stream_decoder_t,
	data *C.uint8_t,
	dataSize C.size_t,
	cframe *C.dlms_hdlc_frame_t,
	infoBuf *C.uint8_t,
	infoBufSize C.size_t,
	infoSize *C.size_t,
) C.dlms_hdlc_status_t {
	if infoSize != nil {
		*infoSize = 0
	}
	if decoder == nil || cframe == nil || infoSize == nil {
		return C.DLMS_HDLC_STATUS_INVALID_ARGUMENT
	}

	state := getStreamHandle(uintptr(decoder._handle))
	if state == nil {
		return C.DLMS_HDLC_STATUS_INVALID_ARGUMENT
	}

	// Feed new bytes when provided.
	if dataSize > 0 {
		if data == nil {
			return C.DLMS_HDLC_STATUS_INVALID_ARGUMENT
		}
		chunk := unsafe.Slice((*byte)(unsafe.Pointer(data)), int(dataSize))
		frames, err := state.decoder.Push(chunk)
		if err != nil && err != hdlc.StatusNeedMoreData {
			return toStatus(err)
		}
		state.pendingFrames = append(state.pendingFrames, frames...)
	}

	if len(state.pendingFrames) == 0 {
		return C.DLMS_HDLC_STATUS_NEED_MORE_DATA
	}

	frame := state.pendingFrames[0]
	state.pendingFrames = state.pendingFrames[1:]

	var ibuf []byte
	if infoBuf != nil && infoBufSize > 0 {
		ibuf = unsafe.Slice((*byte)(unsafe.Pointer(infoBuf)), int(infoBufSize))
	}

	infoLen := len(frame.Information)
	if infoLen > 0 {
		if ibuf == nil || len(ibuf) < infoLen {
			// Put the frame back and report.
			state.pendingFrames = append([]hdlc.Frame{frame}, state.pendingFrames...)
			return C.DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL
		}
		copy(ibuf, frame.Information)
	}

	*infoSize = C.size_t(infoLen)
	return fillCFrame(frame, infoBuf, C.size_t(infoLen), cframe)
}

// ---------------------------------------------------------------------------
// Reassembler lifecycle
// ---------------------------------------------------------------------------

//export dlms_hdlc_reassembler_create
func dlms_hdlc_reassembler_create(
	climits *C.dlms_hdlc_limits_t,
	reassemblerOut **C.dlms_hdlc_reassembler_t,
) C.dlms_hdlc_status_t {
	if reassemblerOut == nil {
		return C.DLMS_HDLC_STATUS_INVALID_ARGUMENT
	}
	*reassemblerOut = nil

	r := hdlc.NewReassembler(toLimits(climits))
	id := newReassemblerHandle(r)

	p := (*C.dlms_hdlc_reassembler_t)(C.malloc(C.sizeof_dlms_hdlc_reassembler_t))
	if p == nil {
		deleteReassemblerHandle(id)
		return C.DLMS_HDLC_STATUS_INTERNAL_ERROR
	}
	p._handle = C.uintptr_t(id)
	*reassemblerOut = p
	return C.DLMS_HDLC_STATUS_OK
}

//export dlms_hdlc_reassembler_destroy
func dlms_hdlc_reassembler_destroy(reassembler *C.dlms_hdlc_reassembler_t) {
	if reassembler == nil {
		return
	}
	deleteReassemblerHandle(uintptr(reassembler._handle))
	C.free(unsafe.Pointer(reassembler))
}

//export dlms_hdlc_reassembler_reset
func dlms_hdlc_reassembler_reset(reassembler *C.dlms_hdlc_reassembler_t) {
	if reassembler == nil {
		return
	}
	r := getReassemblerHandle(uintptr(reassembler._handle))
	if r != nil {
		r.Reset()
	}
}

//export dlms_hdlc_reassembler_push_frame
func dlms_hdlc_reassembler_push_frame(
	reassembler *C.dlms_hdlc_reassembler_t,
	inputFrame *C.dlms_hdlc_frame_t,
	outputFrame *C.dlms_hdlc_frame_t,
	outInfoBuf *C.uint8_t,
	outInfoBufSize C.size_t,
	outInfoSize *C.size_t,
	hasCompleted *C.int,
) C.dlms_hdlc_status_t {
	if outInfoSize != nil {
		*outInfoSize = 0
	}
	if hasCompleted != nil {
		*hasCompleted = 0
	}
	if reassembler == nil || inputFrame == nil || outputFrame == nil || outInfoSize == nil || hasCompleted == nil {
		return C.DLMS_HDLC_STATUS_INVALID_ARGUMENT
	}

	r := getReassemblerHandle(uintptr(reassembler._handle))
	if r == nil {
		return C.DLMS_HDLC_STATUS_INVALID_ARGUMENT
	}

	frame, err := goFrame(inputFrame)
	if err != nil {
		return toStatus(err)
	}

	completed, ok, err := r.Push(frame)
	if err != nil && err != hdlc.StatusSegmentationIncomplete {
		return toStatus(err)
	}
	if !ok {
		return C.DLMS_HDLC_STATUS_SEGMENTATION_INCOMPLETE
	}

	infoLen := len(completed.Information)
	if infoLen > 0 {
		if outInfoBuf == nil || int(outInfoBufSize) < infoLen {
			return C.DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL
		}
		ibuf := unsafe.Slice((*byte)(unsafe.Pointer(outInfoBuf)), int(outInfoBufSize))
		copy(ibuf, completed.Information)
	}

	*outInfoSize = C.size_t(infoLen)
	*hasCompleted = 1
	return fillCFrame(completed, outInfoBuf, C.size_t(infoLen), outputFrame)
}

func main() {}
