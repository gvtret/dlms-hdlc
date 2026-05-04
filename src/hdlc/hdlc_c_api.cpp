#include "dlms/hdlc/hdlc_c_api.h"

#include "dlms/hdlc/hdlc_codec.hpp"
#include "dlms/hdlc/hdlc_segmentation.hpp"
#include "dlms/hdlc/hdlc_stream_decoder.hpp"

#include <algorithm>
#include <new>
#include <vector>

struct dlms_hdlc_stream_decoder_t
{
  explicit dlms_hdlc_stream_decoder_t(
    const dlms::hdlc::HdlcStreamDecoderOptions& options)
    : decoder(options)
  {
  }

  dlms::hdlc::HdlcStreamDecoder decoder;
  std::vector<dlms::hdlc::HdlcFrameBuffer> pending;
};

struct dlms_hdlc_reassembler_t
{
  explicit dlms_hdlc_reassembler_t(
    const dlms::hdlc::HdlcCodecLimits& limits)
    : reassembler(limits)
  {
  }

  dlms::hdlc::HdlcReassembler reassembler;
};

namespace {

dlms_hdlc_status_t ToCStatus(dlms::hdlc::HdlcStatus status)
{
  return static_cast<dlms_hdlc_status_t>(static_cast<int>(status));
}

dlms::hdlc::HdlcCodecLimits ToCppLimits(const dlms_hdlc_limits_t* limits)
{
  dlms::hdlc::HdlcCodecLimits cppLimits =
    dlms::hdlc::DefaultHdlcCodecLimits();

  if (limits != 0) {
    cppLimits.maximumFrameSize = limits->maximum_frame_size;
    cppLimits.maximumInformationFieldSize =
      limits->maximum_information_field_size;
    cppLimits.maximumReassembledInformationSize =
      limits->maximum_reassembled_information_size;
  }

  return cppLimits;
}

dlms::hdlc::HdlcStatus ToCppFrame(
  const dlms_hdlc_frame_t& frame,
  dlms::hdlc::HdlcFrame& cppFrame)
{
  if (frame.information_data == 0 && frame.information_size != 0u) {
    return dlms::hdlc::HdlcStatus::InvalidArgument;
  }

  dlms::hdlc::HdlcStatus status =
    dlms::hdlc::HdlcAddress::FromRaw(frame.destination_address_raw,
                                     frame.destination_address_size,
                                     cppFrame.destination);
  if (status != dlms::hdlc::HdlcStatus::Ok) {
    return status;
  }

  status = dlms::hdlc::HdlcAddress::FromRaw(frame.source_address_raw,
                                            frame.source_address_size,
                                            cppFrame.source);
  if (status != dlms::hdlc::HdlcStatus::Ok) {
    return status;
  }

  status = dlms::hdlc::HdlcControl::Decode(frame.control, cppFrame.control);
  if (status != dlms::hdlc::HdlcStatus::Ok) {
    return status;
  }

  cppFrame.segmented = frame.segmented != 0u;
  cppFrame.informationData = frame.information_data;
  cppFrame.informationSize = frame.information_size;
  return dlms::hdlc::HdlcStatus::Ok;
}

dlms::hdlc::HdlcStatus FromCppFrame(
  const dlms::hdlc::HdlcFrame& cppFrame,
  std::size_t decodedInformationSize,
  dlms_hdlc_frame_t& frame,
  const std::uint8_t* informationBuffer)
{
  frame.segmented = cppFrame.segmented ? 1u : 0u;
  frame.destination_address_raw = cppFrame.destination.RawValue();
  frame.destination_address_size = cppFrame.destination.EncodedSize();
  frame.source_address_raw = cppFrame.source.RawValue();
  frame.source_address_size = cppFrame.source.EncodedSize();

  dlms::hdlc::HdlcStatus status = cppFrame.control.Encode(frame.control);
  if (status != dlms::hdlc::HdlcStatus::Ok) {
    return status;
  }

  frame.information_data = informationBuffer;
  frame.information_size = decodedInformationSize;
  return dlms::hdlc::HdlcStatus::Ok;
}

} // namespace

extern "C" {

dlms_hdlc_status_t dlms_hdlc_encode_frame(
  const dlms_hdlc_frame_t* frame,
  const dlms_hdlc_limits_t* limits,
  uint8_t* output,
  size_t output_size,
  size_t* written_size)
{
  if (written_size != 0) {
    *written_size = 0u;
  }

  if (frame == 0 || output == 0 || written_size == 0) {
    return DLMS_HDLC_STATUS_INVALID_ARGUMENT;
  }

  try {
    dlms::hdlc::HdlcFrame cppFrame;
    dlms::hdlc::HdlcStatus status = ToCppFrame(*frame, cppFrame);
    if (status != dlms::hdlc::HdlcStatus::Ok) {
      return ToCStatus(status);
    }

    std::size_t cppWrittenSize = 0u;
    status = dlms::hdlc::EncodeFrameToBuffer(cppFrame,
                                             ToCppLimits(limits),
                                             output,
                                             output_size,
                                             cppWrittenSize);
    *written_size = cppWrittenSize;
    return ToCStatus(status);
  } catch (...) {
    return DLMS_HDLC_STATUS_INTERNAL_ERROR;
  }
}

dlms_hdlc_status_t dlms_hdlc_decode_frame(
  const uint8_t* input,
  size_t input_size,
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_frame_t* frame,
  uint8_t* information_buffer,
  size_t information_buffer_size,
  size_t* information_size)
{
  if (information_size != 0) {
    *information_size = 0u;
  }

  if (input == 0 || frame == 0 || information_size == 0) {
    return DLMS_HDLC_STATUS_INVALID_ARGUMENT;
  }

  try {
    dlms::hdlc::HdlcFrame cppFrame;
    std::size_t cppInformationSize = 0u;
    dlms::hdlc::HdlcStatus status =
      dlms::hdlc::DecodeFrameFromBuffer(input,
                                        input_size,
                                        ToCppLimits(limits),
                                        cppFrame,
                                        information_buffer,
                                        information_buffer_size,
                                        cppInformationSize);
    if (status != dlms::hdlc::HdlcStatus::Ok) {
      return ToCStatus(status);
    }

    status = FromCppFrame(cppFrame,
                          cppInformationSize,
                          *frame,
                          information_buffer);
    if (status != dlms::hdlc::HdlcStatus::Ok) {
      return ToCStatus(status);
    }

    *information_size = cppInformationSize;
    return DLMS_HDLC_STATUS_OK;
  } catch (...) {
    return DLMS_HDLC_STATUS_INTERNAL_ERROR;
  }
}

dlms_hdlc_status_t dlms_hdlc_stream_decoder_create(
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_stream_decoder_t** decoder)
{
  if (decoder == 0) {
    return DLMS_HDLC_STATUS_INVALID_ARGUMENT;
  }

  *decoder = 0;

  try {
    dlms::hdlc::HdlcStreamDecoderOptions options;
    options.limits = ToCppLimits(limits);
    options.noisePolicy = dlms::hdlc::HdlcNoisePolicy::IgnoreUntilOpeningFlag;
    *decoder = new dlms_hdlc_stream_decoder_t(options);
    return DLMS_HDLC_STATUS_OK;
  } catch (...) {
    return DLMS_HDLC_STATUS_INTERNAL_ERROR;
  }
}

void dlms_hdlc_stream_decoder_destroy(
  dlms_hdlc_stream_decoder_t* decoder)
{
  delete decoder;
}

void dlms_hdlc_stream_decoder_reset(
  dlms_hdlc_stream_decoder_t* decoder)
{
  if (decoder != 0) {
    decoder->decoder.Reset();
    decoder->pending.clear();
  }
}

dlms_hdlc_status_t dlms_hdlc_stream_decoder_push(
  dlms_hdlc_stream_decoder_t* decoder,
  const uint8_t* data,
  size_t data_size,
  dlms_hdlc_frame_t* frame,
  uint8_t* information_buffer,
  size_t information_buffer_size,
  size_t* information_size)
{
  if (information_size != 0) {
    *information_size = 0u;
  }

  if (decoder == 0 || frame == 0 || information_size == 0) {
    return DLMS_HDLC_STATUS_INVALID_ARGUMENT;
  }
  if (data == 0 && data_size != 0u) {
    return DLMS_HDLC_STATUS_INVALID_ARGUMENT;
  }

  try {
    if (data_size > 0u) {
      std::vector<dlms::hdlc::HdlcFrameBuffer> decoded;
      dlms::hdlc::HdlcStatus status = decoder->decoder.Push(data, data_size, decoded);
      if (status != dlms::hdlc::HdlcStatus::Ok &&
          status != dlms::hdlc::HdlcStatus::NeedMoreData) {
        return ToCStatus(status);
      }
      decoder->pending.insert(decoder->pending.end(),
                              std::make_move_iterator(decoded.begin()),
                              std::make_move_iterator(decoded.end()));
    }

    if (decoder->pending.empty()) {
      return DLMS_HDLC_STATUS_NEED_MORE_DATA;
    }

    const dlms::hdlc::HdlcFrameBuffer& f = decoder->pending.front();
    std::size_t infoSize = f.information.size();

    if (infoSize > information_buffer_size) {
      return DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL;
    }
    if (infoSize > 0u && information_buffer != 0) {
      std::copy(f.information.begin(), f.information.end(), information_buffer);
    }
    *information_size = infoSize;

    dlms::hdlc::HdlcFrame view;
    view.segmented = f.segmented;
    view.destination = f.destination;
    view.source = f.source;
    view.control = f.control;
    view.informationData = infoSize > 0u ? information_buffer : 0;
    view.informationSize = infoSize;
    dlms::hdlc::HdlcStatus status = FromCppFrame(view, infoSize, *frame, information_buffer);

    decoder->pending.erase(decoder->pending.begin());
    return ToCStatus(status);
  } catch (...) {
    return DLMS_HDLC_STATUS_INTERNAL_ERROR;
  }
}

dlms_hdlc_status_t dlms_hdlc_reassembler_create(
  const dlms_hdlc_limits_t* limits,
  dlms_hdlc_reassembler_t** reassembler)
{
  if (reassembler == 0) {
    return DLMS_HDLC_STATUS_INVALID_ARGUMENT;
  }

  *reassembler = 0;

  try {
    *reassembler = new dlms_hdlc_reassembler_t(ToCppLimits(limits));
    return DLMS_HDLC_STATUS_OK;
  } catch (...) {
    return DLMS_HDLC_STATUS_INTERNAL_ERROR;
  }
}

void dlms_hdlc_reassembler_destroy(
  dlms_hdlc_reassembler_t* reassembler)
{
  delete reassembler;
}

void dlms_hdlc_reassembler_reset(
  dlms_hdlc_reassembler_t* reassembler)
{
  if (reassembler != 0) {
    reassembler->reassembler.Reset();
  }
}

dlms_hdlc_status_t dlms_hdlc_reassembler_push_frame(
  dlms_hdlc_reassembler_t* reassembler,
  const dlms_hdlc_frame_t* input_frame,
  dlms_hdlc_frame_t* output_frame,
  uint8_t* output_information_buffer,
  size_t output_information_buffer_size,
  size_t* output_information_size,
  int* has_completed_frame)
{
  if (output_information_size != 0) {
    *output_information_size = 0u;
  }
  if (has_completed_frame != 0) {
    *has_completed_frame = 0;
  }

  if (reassembler == 0 || input_frame == 0 || output_frame == 0 ||
      output_information_size == 0 || has_completed_frame == 0) {
    return DLMS_HDLC_STATUS_INVALID_ARGUMENT;
  }

  try {
    dlms::hdlc::HdlcFrame view;
    dlms::hdlc::HdlcStatus status = ToCppFrame(*input_frame, view);
    if (status != dlms::hdlc::HdlcStatus::Ok) {
      return ToCStatus(status);
    }

    dlms::hdlc::HdlcFrameBuffer inBuf;
    inBuf.segmented = view.segmented;
    inBuf.destination = view.destination;
    inBuf.source = view.source;
    inBuf.control = view.control;
    if (view.informationSize > 0u && view.informationData != 0) {
      inBuf.information.assign(view.informationData,
                               view.informationData + view.informationSize);
    }

    dlms::hdlc::HdlcFrameBuffer completed;
    bool hasCompleted = false;
    status = reassembler->reassembler.PushFrame(inBuf, completed, hasCompleted);
    if (status != dlms::hdlc::HdlcStatus::Ok &&
        status != dlms::hdlc::HdlcStatus::SegmentationIncomplete) {
      return ToCStatus(status);
    }
    if (!hasCompleted) {
      return DLMS_HDLC_STATUS_SEGMENTATION_INCOMPLETE;
    }

    std::size_t infoSize = completed.information.size();
    if (infoSize > output_information_buffer_size) {
      return DLMS_HDLC_STATUS_OUTPUT_BUFFER_TOO_SMALL;
    }
    if (infoSize > 0u && output_information_buffer != 0) {
      std::copy(completed.information.begin(), completed.information.end(),
                output_information_buffer);
    }
    *output_information_size = infoSize;
    *has_completed_frame = 1;

    dlms::hdlc::HdlcFrame outView;
    outView.segmented = completed.segmented;
    outView.destination = completed.destination;
    outView.source = completed.source;
    outView.control = completed.control;
    outView.informationData = infoSize > 0u ? output_information_buffer : 0;
    outView.informationSize = infoSize;
    return ToCStatus(FromCppFrame(outView, infoSize, *output_frame,
                                  output_information_buffer));
  } catch (...) {
    return DLMS_HDLC_STATUS_INTERNAL_ERROR;
  }
}

} // extern "C"
