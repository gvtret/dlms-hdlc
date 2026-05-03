#include "dlms/hdlc/hdlc_c_api.h"

int dlms_hdlc_c_header_compiles_as_c(void)
{
  dlms_hdlc_limits_t limits;
  limits.maximum_frame_size = 0u;
  limits.maximum_information_field_size = 0u;
  limits.maximum_reassembled_information_size = 0u;
  return (int)DLMS_HDLC_STATUS_OK;
}
