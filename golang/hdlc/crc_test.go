package hdlc

import "testing"

// referenceBitwiseCRC is a straightforward bitwise implementation used to cross-check
// the table-driven calculateHdlcCRC.
func referenceBitwiseCRC(data []byte) uint16 {
	crc := uint16(0xffff)
	for _, b := range data {
		crc ^= uint16(b)
		for bit := 0; bit < 8; bit++ {
			if crc&0x0001 != 0 {
				crc = (crc >> 1) ^ 0x8408
			} else {
				crc >>= 1
			}
		}
	}
	return crc ^ 0xffff
}

func TestCalculateCrcEmpty(t *testing.T) {
	if got := CalculateHdlcCRC(nil); got != 0x0000 {
		t.Errorf("empty: got 0x%04x, want 0x0000", got)
	}
}

func TestCalculateCrcCanonicalCheckValue(t *testing.T) {
	data := []byte{'1', '2', '3', '4', '5', '6', '7', '8', '9'}
	if got := CalculateHdlcCRC(data); got != 0x906e {
		t.Errorf("canonical: got 0x%04x, want 0x906e", got)
	}
}

func TestCalculateCrcKnownSnrm(t *testing.T) {
	data := []byte{0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93}
	if got := CalculateHdlcCRC(data); got != 0x43e4 {
		t.Errorf("SNRM FCS: got 0x%04x, want 0x43e4", got)
	}
}

func TestCalculateCrcKnownUaHeaderCheckSequence(t *testing.T) {
	data := []byte{0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73}
	if got := CalculateHdlcCRC(data); got != 0x96b4 {
		t.Errorf("UA HCS: got 0x%04x, want 0x96b4", got)
	}
}

func TestCalculateCrcKnownUaFrameCheckSequence(t *testing.T) {
	data := []byte{
		0xa0, 0x1f, 0xc9, 0x02, 0x23, 0x73, 0xb4, 0x96,
		0x81, 0x80, 0x12, 0x05, 0x01, 0x7e, 0x06, 0x01,
		0x7e, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01, 0x08,
		0x04, 0x00, 0x00, 0x00, 0x01,
	}
	if got := CalculateHdlcCRC(data); got != 0x755f {
		t.Errorf("UA FCS: got 0x%04x, want 0x755f", got)
	}
}

func TestValidateHdlcCRCMatch(t *testing.T) {
	data := []byte{0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93}
	if err := ValidateHdlcCRC(data, 0x43e4); err != nil {
		t.Errorf("expected match, got %v", err)
	}
}

func TestValidateHdlcCRCMismatch(t *testing.T) {
	data := []byte{0xa0, 0x08, 0x02, 0x23, 0xc9, 0x93}
	if err := ValidateHdlcCRC(data, 0x43e5); err != StatusInvalidFrameChecksum {
		t.Errorf("expected StatusInvalidFrameChecksum, got %v", err)
	}
}

func TestCalculateCrcMatchesBitwiseReference(t *testing.T) {
	data := []byte{
		0xa0, 0x1a, 0x02, 0x23, 0xc9, 0x32, 0xaf, 0x55,
		0xe6, 0xe6, 0x00, 0xc0, 0x01, 0x40, 0x00, 0x08,
		0x00, 0x00, 0x01, 0x00, 0x00, 0xff, 0x02, 0x00,
	}
	want := referenceBitwiseCRC(data)
	got := CalculateHdlcCRC(data)
	if got != want {
		t.Errorf("CRC: got 0x%04x, want 0x%04x", got, want)
	}
}
