#ifndef DLMS_HDLC_HDLC_C_API_H
#define DLMS_HDLC_HDLC_C_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Stable C ABI status code returned by every C entry point.
 *
 * Numeric values are part of the ABI contract. Do not reorder existing values;
 * add future values only at the end.
 */
typedef enum dlms_hdlc_status_t
{
  /// Operation completed successfully.
  DLMS_HDLC_STATUS_OK = 0,
  /// More input bytes are required before a complete frame is available.
  DLMS_HDLC_STATUS_NEED_MORE_DATA = 1,
  /// Caller-provided output or information buffer is too small.
  DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL = 2,
  /// A pointer, size, option, or argument combination is invalid.
  DLMS_HDLC_STATUS_INVALID_ARGUMENT = 3,
  /// Opening or closing HDLC flag is missing or invalid.
  DLMS_HDLC_STATUS_INVALID_FLAG = 4,
  /// Frame Format field does not contain supported HDLC Type 3 format.
  DLMS_HDLC_STATUS_INVALID_FRAME_FORMAT = 5,
  /// Control field encodes a syntactically unsupported frame type.
  DLMS_HDLC_STATUS_INVALID_FRAME_TYPE = 6,
  /// Frame Format length is inconsistent, too small, or otherwise invalid.
  DLMS_HDLC_STATUS_INVALID_FRAME_LENGTH = 7,
  /// Destination or source HDLC address field is malformed.
  DLMS_HDLC_STATUS_INVALID_ADDRESS = 8,
  /// Control field is malformed or unsupported by the codec.
  DLMS_HDLC_STATUS_INVALID_CONTROL_FIELD = 9,
  /// Header Check Sequence verification failed.
  DLMS_HDLC_STATUS_INVALID_HEADER_CHECKSUM = 10,
  /// Frame Check Sequence verification failed.
  DLMS_HDLC_STATUS_INVALID_FRAME_CHECKSUM = 11,
  /// Frame exceeds configured or format-derived maximum frame size.
  DLMS_HDLC_STATUS_FRAME_TOO_LARGE = 12,
  /// Information field exceeds configured per-frame information limit.
  DLMS_HDLC_STATUS_INFORMATION_FIELD_TOO_LARGE = 13,
  /// Segmented-frame sequence is inconsistent or cannot be continued.
  DLMS_HDLC_STATUS_SEGMENTATION_ERROR = 14,
  /// Segmented sequence is valid so far but more frames are required.
  DLMS_HDLC_STATUS_SEGMENTATION_INCOMPLETE = 15,
  /// Reassembled information would exceed configured reassembly limit.
  DLMS_HDLC_STATUS_SEGMENTATION_OVERFLOW = 16,
  /// Frame is valid HDLC but outside the supported codec feature set.
  DLMS_HDLC_STATUS_UNSUPPORTED_FRAME = 17,
  /// Address is valid HDLC but outside the supported address model.
  DLMS_HDLC_STATUS_UNSUPPORTED_ADDRESS = 18,
  /// Requested feature is intentionally outside this codec layer.
  DLMS_HDLC_STATUS_UNSUPPORTED_FEATURE = 19,
  /// Unexpected internal failure, including allocation failure in wrappers.
  DLMS_HDLC_STATUS_INTERNAL_ERROR = 20
} dlms_hdlc_status_t;

/**
 * @brief Runtime limits passed through the C ABI.
 *
 * A null pointer means "use default limits". Zero fields also select the default
 * for that individual limit.
 */
typedef struct dlms_hdlc_limits_t
{
  size_t maximum_frame_size;
  size_t maximum_information_field_size;
  size_t maximum_reassembled_information_size;
} dlms_hdlc_limits_t;

/**
 * @brief C ABI representation of an HDLC frame.
 *
 * Address fields carry raw 7-bit address-group values without HDLC extension bits.
 * Address sizes are encoded wire sizes in bytes and must be 1, 2, or 4.
 * The Information field is caller-owned memory treated as opaque bytes.
 */
typedef struct dlms_hdlc_frame_t
{
  uint8_t  segmented;
  uint32_t destination_address_raw;
  size_t   destination_address_size;
  uint32_t source_address_raw;
  size_t   source_address_size;
  uint8_t  control;
  const uint8_t* information_data;
  size_t   information_size;
} dlms_hdlc_frame_t;

/**
 * @brief Opaque handle for the incremental stream decoder.
 *
 * The internal layout is implementation-defined. Always use the lifecycle
 * functions below to create, use, and destroy this object.
 */
typedef struct dlms_hdlc_stream_decoder_t
{
  uintptr_t _handle; /* implementation-defined, do not access directly */
} dlms_hdlc_stream_decoder_t;

/**
 * @brief Opaque handle for segmented-frame reassembly.
 */
typedef struct dlms_hdlc_reassembler_t
{
  uintptr_t _handle; /* implementation-defined, do not access directly */
} dlms_hdlc_reassembler_t;

/**
 * @brief Encode one complete HDLC Type 3 frame.
 */
dlms_hdlc_status_t dlms_hdlc_encode_frame(
  const dlms_hdlc_frame_t* frame,
  const dlms_hdlc_limits_t* limits,
  uint8_t* output,
  size_t output_size,
  size_t* written_size);

/**
 * @brief Decode one complete HDLC Type 3 frame.
 *
 * The decoded Information field is copied into information_buffer.
 * frame->information_data will point to that caller-provided buffer.
 */
dlms_hdlc_status_t dlms_hdlc_decode_frame(
  const uint8_t* input,
  size_t input_size,
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_frame_t* frame,
  uint8_t* information_buffer,
  size_t information_buffer_size,
  size_t* information_size);

/**
 * @brief Create an opaque stream decoder handle.
 *
 * The created decoder uses NoisePolicyIgnore by default.
 * Destroy with dlms_hdlc_stream_decoder_destroy.
 */
dlms_hdlc_status_t dlms_hdlc_stream_decoder_create(
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_stream_decoder_t** decoder);

/** @brief Destroy a stream decoder handle. Passing null is safe. */
void dlms_hdlc_stream_decoder_destroy(dlms_hdlc_stream_decoder_t* decoder);

/** @brief Reset a stream decoder to its initial empty state. Passing null is safe. */
void dlms_hdlc_stream_decoder_reset(dlms_hdlc_stream_decoder_t* decoder);

/**
 * @brief Push bytes into the stream decoder and receive one decoded frame per call.
 *
 * Call repeatedly with data=NULL, data_size=0 to drain queued frames after the
 * first call with actual data. Returns DLMS_HDLC_STATUS_NEED_MORE_DATA when no
 * complete frame is available.
 */
dlms_hdlc_status_t dlms_hdlc_stream_decoder_push(
  dlms_hdlc_stream_decoder_t* decoder,
  const uint8_t* data,
  size_t data_size,
  dlms_hdlc_frame_t* frame,
  uint8_t* information_buffer,
  size_t information_buffer_size,
  size_t* information_size);

/**
 * @brief Create an opaque reassembler handle.
 * Destroy with dlms_hdlc_reassembler_destroy.
 */
dlms_hdlc_status_t dlms_hdlc_reassembler_create(
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_reassembler_t** reassembler);

/** @brief Destroy a reassembler handle. Passing null is safe. */
void dlms_hdlc_reassembler_destroy(dlms_hdlc_reassembler_t* reassembler);

/** @brief Reset a reassembler to an empty pending sequence. Passing null is safe. */
void dlms_hdlc_reassembler_reset(dlms_hdlc_reassembler_t* reassembler);

/**
 * @brief Push one decoded frame into the reassembler.
 *
 * When has_completed_frame is set to 1, output_frame and output_information_buffer
 * hold the completed reassembled frame.
 *
 * Returns DLMS_HDLC_STATUS_OK with has_completed_frame=1 when complete,
 * DLMS_HDLC_STATUS_SEGMENTATION_INCOMPLETE when more frames are needed,
 * or an error status on failure.
 */
dlms_hdlc_status_t dlms_hdlc_reassembler_push_frame(
  dlms_hdlc_reassembler_t* reassembler,
  const dlms_hdlc_frame_t* input_frame,
  dlms_hdlc_frame_t* output_frame,
  uint8_t* output_information_buffer,
  size_t output_information_buffer_size,
  size_t* output_information_size,
  int* has_completed_frame);

#ifdef __cplusplus
}
#endif

#endif /* DLMS_HDLC_HDLC_C_API_H */
