package hdlc_test

import (
	"testing"

	"dlms-hdlc/hdlc"
)

func TestStatusValuesMatchDocumentedOrder(t *testing.T) {
	cases := []struct {
		s    hdlc.Status
		want int
	}{
		{hdlc.StatusOk, 0},
		{hdlc.StatusNeedMoreData, 1},
		{hdlc.StatusOutputBufferTooSmall, 2},
		{hdlc.StatusInvalidArgument, 3},
		{hdlc.StatusInvalidFlag, 4},
		{hdlc.StatusInvalidFrameFormat, 5},
		{hdlc.StatusInvalidFrameType, 6},
		{hdlc.StatusInvalidFrameLength, 7},
		{hdlc.StatusInvalidAddress, 8},
		{hdlc.StatusInvalidControlField, 9},
		{hdlc.StatusInvalidHeaderChecksum, 10},
		{hdlc.StatusInvalidFrameChecksum, 11},
		{hdlc.StatusFrameTooLarge, 12},
		{hdlc.StatusInformationFieldTooLarge, 13},
		{hdlc.StatusSegmentationError, 14},
		{hdlc.StatusSegmentationIncomplete, 15},
		{hdlc.StatusSegmentationOverflow, 16},
		{hdlc.StatusUnsupportedFrame, 17},
		{hdlc.StatusUnsupportedAddress, 18},
		{hdlc.StatusUnsupportedFeature, 19},
		{hdlc.StatusInternalError, 20},
	}
	for _, tc := range cases {
		if int(tc.s) != tc.want {
			t.Errorf("Status %v: got %d, want %d", tc.s, int(tc.s), tc.want)
		}
	}
}
