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
 * A null `dlms_hdlc_limits_t*` means "use default limits". Individual zero
 * fields also keep the C++ codec's default behavior for that limit.
 */
typedef struct dlms_hdlc_limits_t
{
  /// Maximum complete HDLC frame size in bytes, including both flag bytes.
  size_t maximum_frame_size;
  /// Maximum Information field size accepted in a single HDLC frame.
  size_t maximum_information_field_size;
  /// Maximum accumulated Information size accepted by reassembly.
  size_t maximum_reassembled_information_size;
} dlms_hdlc_limits_t;

/**
 * @brief C ABI representation of an HDLC frame.
 *
 * Address fields use raw 7-bit address-group values without HDLC extension
 * bits. Address sizes are encoded wire sizes in bytes and must be 1, 2, or 4.
 * The Information field is caller-owned memory and is treated as opaque bytes.
 */
typedef struct dlms_hdlc_frame_t
{
  /// HDLC Frame Format segmentation bit; non-zero means true.
  uint8_t segmented;

  /// Destination address raw value without HDLC extension bits.
  uint32_t destination_address_raw;
  /// Destination address encoded wire size in bytes: 1, 2, or 4.
  size_t destination_address_size;

  /// Source address raw value without HDLC extension bits.
  uint32_t source_address_raw;
  /// Source address encoded wire size in bytes: 1, 2, or 4.
  size_t source_address_size;

  /// Raw HDLC control field byte.
  uint8_t control;

  /// Pointer to opaque Information field bytes, or null when empty.
  const uint8_t* information_data;
  /// Number of bytes available at `information_data`.
  size_t information_size;
} dlms_hdlc_frame_t;

/**
 * @brief Opaque C ABI handle for the incremental stream decoder.
 *
 * The internal layout is implementation-defined. Always use the lifecycle
 * functions below to create, use, and destroy this object.
 */
typedef struct dlms_hdlc_stream_decoder_t dlms_hdlc_stream_decoder_t;
/**
 * @brief Opaque C ABI handle for segmented-frame reassembly.
 *
 * The internal layout is implementation-defined. Always use the lifecycle
 * functions below to create, use, and destroy this object.
 */
typedef struct dlms_hdlc_reassembler_t dlms_hdlc_reassembler_t;

/**
 * @brief Encode one complete HDLC Type 3 frame through the C ABI.
 *
 * The function writes a complete frame including opening and closing flags into
 * caller-provided storage. It never throws across the ABI boundary.
 *
 * @param frame Frame description to encode; must not be null.
 * @param limits Optional limits; null selects defaults.
 * @param output Destination buffer; must not be null.
 * @param output_size Size of `output` in bytes.
 * @param written_size Receives encoded byte count; must not be null.
 * @return Stable C ABI status code.
 */
dlms_hdlc_status_t dlms_hdlc_encode_frame(
  const dlms_hdlc_frame_t* frame,
  const dlms_hdlc_limits_t* limits,
  uint8_t* output,
  size_t output_size,
  size_t* written_size);

/**
 * @brief Decode one complete HDLC Type 3 frame through the C ABI.
 *
 * The decoded Information field is copied into `information_buffer`. The
 * returned `frame->information_data` points to that caller-provided buffer.
 *
 * @param input Complete frame bytes including opening and closing flags.
 * @param input_size Number of bytes at `input`.
 * @param limits Optional limits; null selects defaults.
 * @param frame Receives decoded frame fields; must not be null.
 * @param information_buffer Storage for decoded Information bytes.
 * @param information_buffer_size Size of `information_buffer` in bytes.
 * @param information_size Receives decoded Information byte count.
 * @return Stable C ABI status code.
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
 * The created decoder uses default leading-noise behavior: ignore bytes until
 * the next opening flag. Destroy the handle with
 * `dlms_hdlc_stream_decoder_destroy`.
 *
 * @param limits Optional limits; null selects defaults.
 * @param decoder Receives the created handle; must not be null.
 * @return `DLMS_HDLC_STATUS_OK` or an error status.
 */
dlms_hdlc_status_t dlms_hdlc_stream_decoder_create(
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_stream_decoder_t** decoder);

/**
 * @brief Destroy a stream decoder handle.
 *
 * Passing null is allowed and has no effect.
 *
 * @param decoder Handle returned by `dlms_hdlc_stream_decoder_create`.
 */
void dlms_hdlc_stream_decoder_destroy(
  dlms_hdlc_stream_decoder_t* decoder);

/**
 * @brief Reset a stream decoder handle to its initial empty state.
 *
 * Discards all buffered input and any decoded frames waiting to be drained.
 * Passing null is allowed and has no effect.
 *
 * @param decoder Handle returned by `dlms_hdlc_stream_decoder_create`.
 */
void dlms_hdlc_stream_decoder_reset(
  dlms_hdlc_stream_decoder_t* decoder);

/**
 * @brief Push bytes into the stream decoder and receive one decoded frame.
 *
 * Feed new data with `data` and `data_size`; pass `data_size == 0` to drain
 * previously decoded frames without providing more input. Returns
 * `DLMS_HDLC_STATUS_NEED_MORE_DATA` when no complete frame is available yet.
 * Returns `DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL` when the decoded
 * Information field does not fit in `information_buffer`.
 * On error the decoder is reset.
 *
 * @param decoder Handle returned by `dlms_hdlc_stream_decoder_create`; must not be null.
 * @param data Incoming byte chunk; may be null only when `data_size == 0`.
 * @param data_size Number of bytes at `data`.
 * @param frame Receives one decoded frame; must not be null.
 * @param information_buffer Caller-provided storage for the Information field.
 * @param information_buffer_size Size of `information_buffer` in bytes.
 * @param information_size Receives the decoded Information byte count; must not be null.
 * @return Stable C ABI status code.
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
 * @brief Create an opaque segmented-frame reassembler handle.
 *
 * Destroy the handle with `dlms_hdlc_reassembler_destroy`.
 *
 * @param limits Optional limits; null selects defaults.
 * @param reassembler Receives the created handle; must not be null.
 * @return `DLMS_HDLC_STATUS_OK` or an error status.
 */
dlms_hdlc_status_t dlms_hdlc_reassembler_create(
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_reassembler_t** reassembler);

/**
 * @brief Destroy a reassembler handle.
 *
 * Passing null is allowed and has no effect.
 *
 * @param reassembler Handle returned by `dlms_hdlc_reassembler_create`.
 */
void dlms_hdlc_reassembler_destroy(
  dlms_hdlc_reassembler_t* reassembler);

/**
 * @brief Reset a reassembler handle to an empty pending sequence.
 *
 * Passing null is allowed and has no effect.
 *
 * @param reassembler Handle returned by `dlms_hdlc_reassembler_create`.
 */
void dlms_hdlc_reassembler_reset(
  dlms_hdlc_reassembler_t* reassembler);

/**
 * @brief Push one decoded frame into the reassembler.
 *
 * A non-segmented frame with no pending sequence completes immediately.
 * A segmented frame starts or continues a sequence and returns
 * `DLMS_HDLC_STATUS_SEGMENTATION_INCOMPLETE` until the final non-segmented
 * frame arrives.
 * When `*has_completed_frame` is set to 1 on return, `output_frame` and
 * `output_information_buffer` hold the completed reassembled frame.
 *
 * @param reassembler Handle returned by `dlms_hdlc_reassembler_create`; must not be null.
 * @param input_frame Decoded segmented frame to push; must not be null.
 * @param output_frame Receives the completed frame when reassembly finishes; must not be null.
 * @param output_information_buffer Caller-provided storage for the reassembled Information field.
 * @param output_information_buffer_size Size of `output_information_buffer` in bytes.
 * @param output_information_size Receives the reassembled Information byte count; must not be null.
 * @param has_completed_frame Set to 1 when a completed frame is available; must not be null.
 * @return `DLMS_HDLC_STATUS_OK`, `DLMS_HDLC_STATUS_SEGMENTATION_INCOMPLETE`, or an error status.
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

#endif
