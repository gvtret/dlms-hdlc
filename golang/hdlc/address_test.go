package hdlc_test

import (
	"bytes"
	"testing"

	"dlms-hdlc/hdlc"
)

func TestDecodeOneByte(t *testing.T) {
	addr, consumed, err := hdlc.AddressFromBytes([]byte{0x75})
	if err != nil {
		t.Fatal(err)
	}
	if consumed != 1 {
		t.Errorf("consumed: got %d, want 1", consumed)
	}
	if addr.RawValue() != 0x3a {
		t.Errorf("raw: got 0x%x, want 0x3a", addr.RawValue())
	}
	if addr.EncodedSize() != 1 {
		t.Errorf("encodedSize: got %d, want 1", addr.EncodedSize())
	}
}

func TestDecodeTwoBytes(t *testing.T) {
	addr, consumed, err := hdlc.AddressFromBytes([]byte{0x00, 0x21})
	if err != nil {
		t.Fatal(err)
	}
	if consumed != 2 {
		t.Errorf("consumed: got %d, want 2", consumed)
	}
	if addr.RawValue() != 0x0010 {
		t.Errorf("raw: got 0x%x, want 0x0010", addr.RawValue())
	}
	if addr.EncodedSize() != 2 {
		t.Errorf("encodedSize: got %d, want 2", addr.EncodedSize())
	}
}

func TestDecodeFourBytes(t *testing.T) {
	addr, consumed, err := hdlc.AddressFromBytes([]byte{0x48, 0x68, 0xfe, 0xff})
	if err != nil {
		t.Fatal(err)
	}
	if consumed != 4 {
		t.Errorf("consumed: got %d, want 4", consumed)
	}
	if addr.RawValue() != 0x048d3fff {
		t.Errorf("raw: got 0x%x, want 0x048d3fff", addr.RawValue())
	}
	if addr.EncodedSize() != 4 {
		t.Errorf("encodedSize: got %d, want 4", addr.EncodedSize())
	}
}

func TestDecodeTruncatedAddress(t *testing.T) {
	_, consumed, err := hdlc.AddressFromBytes([]byte{0x48, 0x68})
	if err != hdlc.StatusNeedMoreData {
		t.Errorf("expected NeedMoreData, got %v", err)
	}
	if consumed != 0 {
		t.Errorf("consumed: got %d, want 0", consumed)
	}
}

func TestDecodeInvalidExtensionBit(t *testing.T) {
	_, consumed, err := hdlc.AddressFromBytes([]byte{0x48, 0x68, 0xfe, 0xfe})
	if err != hdlc.StatusInvalidAddress {
		t.Errorf("expected InvalidAddress, got %v", err)
	}
	if consumed != 0 {
		t.Errorf("consumed: got %d, want 0", consumed)
	}
}

func TestEncodeOneByte(t *testing.T) {
	addr, err := hdlc.AddressFromRaw(0x3a, 1)
	if err != nil {
		t.Fatal(err)
	}
	if wire := addr.Encode(); !bytes.Equal(wire, []byte{0x75}) {
		t.Errorf("encode: got %x, want [75]", wire)
	}
}

func TestEncodeTwoBytes(t *testing.T) {
	addr, err := hdlc.AddressFromRaw(0x0010, 2)
	if err != nil {
		t.Fatal(err)
	}
	if wire := addr.Encode(); !bytes.Equal(wire, []byte{0x00, 0x21}) {
		t.Errorf("encode: got %x, want [00 21]", wire)
	}
}

func TestEncodeFourBytes(t *testing.T) {
	addr, err := hdlc.AddressFromRaw(0x048d3fff, 4)
	if err != nil {
		t.Fatal(err)
	}
	if wire := addr.Encode(); !bytes.Equal(wire, []byte{0x48, 0x68, 0xfe, 0xff}) {
		t.Errorf("encode: got %x, want [48 68 fe ff]", wire)
	}
}

func TestAddressFromRawRejectsUnsupportedSize(t *testing.T) {
	_, err := hdlc.AddressFromRaw(0, 3)
	if err != hdlc.StatusUnsupportedAddress {
		t.Errorf("expected UnsupportedAddress, got %v", err)
	}
}

func TestAddressFromRawRejectsValueTooLargeForSize(t *testing.T) {
	_, err := hdlc.AddressFromRaw(0x80, 1)
	if err != hdlc.StatusInvalidAddress {
		t.Errorf("expected InvalidAddress, got %v", err)
	}
}

func TestMakeClientAddressValid(t *testing.T) {
	addr, err := hdlc.MakeClientAddress(0x10)
	if err != nil {
		t.Fatal(err)
	}
	if wire := addr.Encode(); !bytes.Equal(wire, []byte{0x21}) {
		t.Errorf("encode: got %x, want [21]", wire)
	}
}

func TestMakeClientAddressRejectsExtendedRange(t *testing.T) {
	_, err := hdlc.MakeClientAddress(0x80)
	if err != hdlc.StatusInvalidAddress {
		t.Errorf("expected InvalidAddress, got %v", err)
	}
}

func TestMakeServerAddressLogicalOnly(t *testing.T) {
	addr, err := hdlc.MakeServerAddress(0x01, 0x00)
	if err != nil {
		t.Fatal(err)
	}
	if wire := addr.Encode(); !bytes.Equal(wire, []byte{0x03}) {
		t.Errorf("encode: got %x, want [03]", wire)
	}
}

func TestMakeServerAddressLogicalPhysical(t *testing.T) {
	addr, err := hdlc.MakeServerAddress(0x01, 0x10)
	if err != nil {
		t.Fatal(err)
	}
	if wire := addr.Encode(); !bytes.Equal(wire, []byte{0x02, 0x21}) {
		t.Errorf("encode: got %x, want [02 21]", wire)
	}
}

func TestMakeServerAddressFourByteLogicalPhysical(t *testing.T) {
	addr, err := hdlc.MakeServerAddress(0x1234, 0x3fff)
	if err != nil {
		t.Fatal(err)
	}
	if wire := addr.Encode(); !bytes.Equal(wire, []byte{0x48, 0x68, 0xfe, 0xff}) {
		t.Errorf("encode: got %x, want [48 68 fe ff]", wire)
	}
}
