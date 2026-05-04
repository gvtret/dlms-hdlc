package hdlc

// CRC-16/HDLC parameters used by both HCS and FCS fields:
// initial value 0xFFFF, reflected polynomial 0x8408 (= 0x1021 bit-reversed),
// input and output reflected, final XOR 0xFFFF.
// The returned integer is the numeric CRC value; HDLC frames store it
// low byte first (little-endian).
const (
	crcInitial    uint16 = 0xffff
	crcPolynomial uint16 = 0x8408
)

func updateCRC(crc uint16, value uint8) uint16 {
	crc ^= uint16(value)
	for bit := 0; bit < 8; bit++ {
		if crc&0x0001 != 0 {
			crc = (crc >> 1) ^ crcPolynomial
		} else {
			crc >>= 1
		}
	}
	return crc
}

func calculateHdlcCRC(data []byte) uint16 {
	crc := crcInitial
	for _, b := range data {
		crc = updateCRC(crc, b)
	}
	return crc ^ 0xffff
}

// CalculateHdlcCRC computes the DLMS/COSEM HDLC 16-bit CRC value over data.
// Uses the reflected HDLC FCS-16 algorithm: polynomial 0x8408, initial value
// 0xFFFF, final XOR 0xFFFF. The returned value is stored in HDLC frames low
// byte first (little-endian).
func CalculateHdlcCRC(data []byte) uint16 {
	return calculateHdlcCRC(data)
}

// ValidateHdlcCRC validates data against an expected HDLC CRC value.
// Returns nil when the CRC matches, StatusInvalidFrameChecksum otherwise.
func ValidateHdlcCRC(data []byte, expected uint16) error {
	if calculateHdlcCRC(data) != expected {
		return StatusInvalidFrameChecksum
	}
	return nil
}
