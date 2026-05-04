package hdlc

// Address holds an encoded DLMS/COSEM HDLC address.
// It stores the address as a raw concatenation of 7-bit address groups with
// HDLC extension bits stripped, together with the encoded wire size in bytes.
// Supported encoded sizes are 1, 2, and 4 bytes.
type Address struct {
	rawValue    uint32
	encodedSize uint
}

func isSupportedEncodedSize(n uint) bool {
	return n == 1 || n == 2 || n == 4
}

func maximumRawValue(encodedSize uint) uint32 {
	return (uint32(1) << (encodedSize * 7)) - 1
}

// AddressFromBytes decodes an HDLC address field from wire bytes.
// The decoder consumes bytes until it finds the extension bit set in the final
// address byte. Only 1-, 2-, and 4-byte encoded addresses are accepted.
// Returns the decoded Address, the number of bytes consumed, and an error.
// Returns StatusNeedMoreData when the data is truncated, StatusInvalidAddress
// when four bytes have been consumed without a terminal bit.
func AddressFromBytes(data []byte) (Address, int, error) {
	var rawValue uint32
	for i := 0; i < len(data) && i < 4; i++ {
		rawValue = (rawValue << 7) | uint32((data[i]>>1)&0x7f)
		consumed := uint(i + 1)
		if data[i]&0x01 != 0 {
			if !isSupportedEncodedSize(consumed) {
				return Address{}, 0, StatusUnsupportedAddress
			}
			return Address{rawValue: rawValue, encodedSize: consumed}, int(consumed), nil
		}
	}
	if len(data) < 4 {
		return Address{}, 0, StatusNeedMoreData
	}
	return Address{}, 0, StatusInvalidAddress
}

// AddressFromRaw constructs an Address from a raw value and encoded wire size.
// encodedSize must be 1, 2, or 4; any other value returns StatusUnsupportedAddress.
// Returns StatusInvalidAddress when rawValue exceeds the range representable in
// encodedSize bytes.
func AddressFromRaw(rawValue uint32, encodedSize uint) (Address, error) {
	if !isSupportedEncodedSize(encodedSize) {
		return Address{}, StatusUnsupportedAddress
	}
	if rawValue > maximumRawValue(encodedSize) {
		return Address{}, StatusInvalidAddress
	}
	return Address{rawValue: rawValue, encodedSize: encodedSize}, nil
}

// Encode returns a new byte slice containing the wire-format HDLC address.
// The slice length equals EncodedSize(). Each byte's seven high bits carry a
// 7-bit address group; bit 0 is the extension bit (1 in the final byte only).
func (a Address) Encode() []byte {
	out := make([]byte, a.encodedSize)
	for i := uint(0); i < a.encodedSize; i++ {
		shift := (a.encodedSize - i - 1) * 7
		val := byte((a.rawValue >> shift) & 0x7f)
		val <<= 1
		if i+1 == a.encodedSize {
			val |= 0x01
		}
		out[i] = val
	}
	return out
}

// RawValue returns the concatenated 7-bit address groups without extension bits.
func (a Address) RawValue() uint32 { return a.rawValue }

// EncodedSize returns the wire encoding size of this address in bytes: 1, 2, or 4.
func (a Address) EncodedSize() uint { return a.encodedSize }

// MakeClientAddress builds a one-byte DLMS client HDLC address.
// clientAddr must be in the range 0x00–0x7F; values above 0x7F return
// StatusInvalidAddress.
func MakeClientAddress(clientAddr uint8) (Address, error) {
	if clientAddr > 0x7f {
		return Address{}, StatusInvalidAddress
	}
	return AddressFromRaw(uint32(clientAddr), 1)
}

// MakeServerAddress builds a DLMS server HDLC address from logical and physical
// device address components, following DLMS/COSEM HDLC address sizing rules.
// Produces a logical-only 1-byte address when physicalAddr is zero and
// logicalAddr fits in 7 bits; otherwise produces a 2- or 4-byte address.
// Both components must be at most 0x3FFF; larger values return StatusInvalidAddress.
func MakeServerAddress(logicalAddr, physicalAddr uint16) (Address, error) {
	if logicalAddr > 0x3fff || physicalAddr > 0x3fff {
		return Address{}, StatusInvalidAddress
	}
	if physicalAddr == 0 && logicalAddr <= 0x7f {
		return AddressFromRaw(uint32(logicalAddr), 1)
	}
	if logicalAddr <= 0x7f && physicalAddr <= 0x7f {
		raw := (uint32(logicalAddr) << 7) | uint32(physicalAddr)
		return AddressFromRaw(raw, 2)
	}
	raw := (uint32(logicalAddr) << 14) | uint32(physicalAddr)
	return AddressFromRaw(raw, 4)
}
