#ifndef DLMS_HDLC_HDLC_C_API_H
#define DLMS_HDLC_HDLC_C_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dlms_hdlc_status_t
{
  DLMS_HDLC_STATUS_OK = 0,
  DLMS_HDLC_STATUS_NEED_MORE_DATA = 1,
  DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL = 2,
  DLMS_HDLC_STATUS_INVALID_ARGUMENT = 3,
  DLMS_HDLC_STATUS_INVALID_FLAG = 4,
  DLMS_HDLC_STATUS_INVALID_FRAME_FORMAT = 5,
  DLMS_HDLC_STATUS_INVALID_FRAME_TYPE = 6,
  DLMS_HDLC_STATUS_INVALID_FRAME_LENGTH = 7,
  DLMS_HDLC_STATUS_INVALID_ADDRESS = 8,
  DLMS_HDLC_STATUS_INVALID_CONTROL_FIELD = 9,
  DLMS_HDLC_STATUS_INVALID_HEADER_CHECKSUM = 10,
  DLMS_HDLC_STATUS_INVALID_FRAME_CHECKSUM = 11,
  DLMS_HDLC_STATUS_FRAME_TOO_LARGE = 12,
  DLMS_HDLC_STATUS_INFORMATION_FIELD_TOO_LARGE = 13,
  DLMS_HDLC_STATUS_SEGMENTATION_ERROR = 14,
  DLMS_HDLC_STATUS_SEGMENTATION_INCOMPLETE = 15,
  DLMS_HDLC_STATUS_SEGMENTATION_OVERFLOW = 16,
  DLMS_HDLC_STATUS_UNSUPPORTED_FRAME = 17,
  DLMS_HDLC_STATUS_UNSUPPORTED_ADDRESS = 18,
  DLMS_HDLC_STATUS_UNSUPPORTED_FEATURE = 19,
  DLMS_HDLC_STATUS_INTERNAL_ERROR = 20
} dlms_hdlc_status_t;

typedef struct dlms_hdlc_limits_t
{
  size_t maximum_frame_size;
  size_t maximum_information_field_size;
  size_t maximum_reassembled_information_size;
} dlms_hdlc_limits_t;

typedef struct dlms_hdlc_frame_t
{
  uint8_t segmented;

  uint32_t destination_address_raw;
  size_t destination_address_size;

  uint32_t source_address_raw;
  size_t source_address_size;

  uint8_t control;

  const uint8_t* information_data;
  size_t information_size;
} dlms_hdlc_frame_t;

typedef struct dlms_hdlc_stream_decoder_t dlms_hdlc_stream_decoder_t;
typedef struct dlms_hdlc_reassembler_t dlms_hdlc_reassembler_t;

dlms_hdlc_status_t dlms_hdlc_encode_frame(
  const dlms_hdlc_frame_t* frame,
  const dlms_hdlc_limits_t* limits,
  uint8_t* output,
  size_t output_size,
  size_t* written_size);

dlms_hdlc_status_t dlms_hdlc_decode_frame(
  const uint8_t* input,
  size_t input_size,
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_frame_t* frame,
  uint8_t* information_buffer,
  size_t information_buffer_size,
  size_t* information_size);

dlms_hdlc_status_t dlms_hdlc_stream_decoder_create(
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_stream_decoder_t** decoder);

void dlms_hdlc_stream_decoder_destroy(
  dlms_hdlc_stream_decoder_t* decoder);

void dlms_hdlc_stream_decoder_reset(
  dlms_hdlc_stream_decoder_t* decoder);

dlms_hdlc_status_t dlms_hdlc_reassembler_create(
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_reassembler_t** reassembler);

void dlms_hdlc_reassembler_destroy(
  dlms_hdlc_reassembler_t* reassembler);

void dlms_hdlc_reassembler_reset(
  dlms_hdlc_reassembler_t* reassembler);

#ifdef __cplusplus
}
#endif

#endif
