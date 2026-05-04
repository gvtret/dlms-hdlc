package hdlc_test

import (
	"testing"

	"dlms-hdlc/hdlc"
)

func makeSessionOptions(t *testing.T, role hdlc.SessionRole) hdlc.SessionOptions {
	t.Helper()
	client, err := hdlc.MakeClientAddress(0x64)
	if err != nil {
		t.Fatal(err)
	}
	server, err := hdlc.MakeServerAddress(0x01, 0x11)
	if err != nil {
		t.Fatal(err)
	}
	return hdlc.SessionOptions{
		Role:          role,
		ClientAddress: client,
		ServerAddress: server,
		Limits:        hdlc.DefaultCodecLimits(),
	}
}

func decodeOrFail(t *testing.T, data []byte) hdlc.Frame {
	t.Helper()
	frame, err := hdlc.DecodeFrame(data, hdlc.DefaultCodecLimits())
	if err != nil {
		t.Fatalf("DecodeFrame: %v", err)
	}
	return frame
}

func establish(t *testing.T, client, server *hdlc.Session) {
	t.Helper()

	connReq, err := client.BuildConnectRequest()
	if err != nil {
		t.Fatalf("BuildConnectRequest: %v", err)
	}
	if err := server.ReceiveFrame(decodeOrFail(t, connReq)); err != nil {
		t.Fatalf("server.ReceiveFrame(SNRM): %v", err)
	}

	connResp, err := server.BuildConnectResponse()
	if err != nil {
		t.Fatalf("BuildConnectResponse: %v", err)
	}
	if err := client.ReceiveFrame(decodeOrFail(t, connResp)); err != nil {
		t.Fatalf("client.ReceiveFrame(UA): %v", err)
	}

	if client.State() != hdlc.SessionStateConnected {
		t.Fatalf("client state: got %v, want Connected", client.State())
	}
	if server.State() != hdlc.SessionStateConnected {
		t.Fatalf("server state: got %v, want Connected", server.State())
	}
}

func TestClientBuildsSnrmConnectRequest(t *testing.T) {
	client := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleClient))

	connReq, err := client.BuildConnectRequest()
	if err != nil {
		t.Fatal(err)
	}

	frame := decodeOrFail(t, connReq)
	if client.State() != hdlc.SessionStateAwaitingConnection {
		t.Errorf("state: got %v, want AwaitingConnection", client.State())
	}
	if frame.Control.FrameKind() != hdlc.FrameKindUnnumbered {
		t.Errorf("frameKind: got %v, want Unnumbered", frame.Control.FrameKind())
	}
	if !frame.Control.PollFinal() {
		t.Error("PollFinal: want true")
	}
	if frame.Destination.RawValue() != 0x91 {
		t.Errorf("dst raw: got 0x%x, want 0x91", frame.Destination.RawValue())
	}
	if frame.Source.RawValue() != 0x64 {
		t.Errorf("src raw: got 0x%x, want 0x64", frame.Source.RawValue())
	}
}

func TestServerAcceptsSnrmAndBuildsUa(t *testing.T) {
	client := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleClient))
	server := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleServer))

	connReq, err := client.BuildConnectRequest()
	if err != nil {
		t.Fatal(err)
	}
	if err := server.ReceiveFrame(decodeOrFail(t, connReq)); err != nil {
		t.Fatalf("server.ReceiveFrame: %v", err)
	}

	connResp, err := server.BuildConnectResponse()
	if err != nil {
		t.Fatal(err)
	}

	frame := decodeOrFail(t, connResp)
	if server.State() != hdlc.SessionStateConnected {
		t.Errorf("state: got %v, want Connected", server.State())
	}
	if frame.Control.FrameKind() != hdlc.FrameKindUnnumbered {
		t.Errorf("frameKind: got %v, want Unnumbered", frame.Control.FrameKind())
	}
	if !frame.Control.PollFinal() {
		t.Error("PollFinal: want true")
	}
	if frame.Destination.RawValue() != 0x64 {
		t.Errorf("dst raw: got 0x%x, want 0x64", frame.Destination.RawValue())
	}
	if frame.Source.RawValue() != 0x91 {
		t.Errorf("src raw: got 0x%x, want 0x91", frame.Source.RawValue())
	}
}

func TestClientConnectsAfterUa(t *testing.T) {
	client := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleClient))
	server := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleServer))
	establish(t, client, server)

	if client.SendSequence() != 0 {
		t.Errorf("sendSeq: got %d, want 0", client.SendSequence())
	}
	if client.ReceiveSequence() != 0 {
		t.Errorf("recvSeq: got %d, want 0", client.ReceiveSequence())
	}
}

func TestInformationFramesAdvanceSequences(t *testing.T) {
	client := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleClient))
	server := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleServer))
	establish(t, client, server)

	information := []byte{0xe6, 0xe6, 0x00}
	iBytes, err := client.BuildInformationFrame(information, true)
	if err != nil {
		t.Fatal(err)
	}
	if client.SendSequence() != 1 {
		t.Errorf("client sendSeq after send: got %d, want 1", client.SendSequence())
	}

	frame := decodeOrFail(t, iBytes)
	if frame.Control.FrameKind() != hdlc.FrameKindInformation {
		t.Errorf("frameKind: got %v, want Information", frame.Control.FrameKind())
	}
	if frame.Control.SendSequence() != 0 {
		t.Errorf("N(S): got %d, want 0", frame.Control.SendSequence())
	}
	if frame.Control.ReceiveSequence() != 0 {
		t.Errorf("N(R): got %d, want 0", frame.Control.ReceiveSequence())
	}

	if err := server.ReceiveFrame(frame); err != nil {
		t.Fatalf("server.ReceiveFrame: %v", err)
	}
	if server.ReceiveSequence() != 1 {
		t.Errorf("server recvSeq: got %d, want 1", server.ReceiveSequence())
	}
}

func TestReceiveReadyAcknowledgesSentInformation(t *testing.T) {
	client := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleClient))
	server := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleServer))
	establish(t, client, server)

	information := []byte{0xe6, 0xe6, 0x00}
	iBytes, err := client.BuildInformationFrame(information, false)
	if err != nil {
		t.Fatal(err)
	}
	if err := server.ReceiveFrame(decodeOrFail(t, iBytes)); err != nil {
		t.Fatal(err)
	}

	rrBytes, err := server.BuildReceiveReadyFrame(true)
	if err != nil {
		t.Fatal(err)
	}
	rr := decodeOrFail(t, rrBytes)

	if rr.Control.FrameKind() != hdlc.FrameKindSupervisory {
		t.Errorf("frameKind: got %v, want Supervisory", rr.Control.FrameKind())
	}
	if rr.Control.ReceiveSequence() != 1 {
		t.Errorf("N(R): got %d, want 1", rr.Control.ReceiveSequence())
	}
	if err := client.ReceiveFrame(rr); err != nil {
		t.Fatalf("client.ReceiveFrame(RR): %v", err)
	}
}

func TestRejectsUnexpectedInformationSequence(t *testing.T) {
	client := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleClient))
	server := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleServer))
	establish(t, client, server)

	iBytes, err := client.BuildInformationFrame([]byte{0xe6}, false)
	if err != nil {
		t.Fatal(err)
	}

	frame := decodeOrFail(t, iBytes)
	if err := server.ReceiveFrame(frame); err != nil {
		t.Fatalf("first receive: %v", err)
	}
	if err := server.ReceiveFrame(frame); err != hdlc.StatusInvalidControlField {
		t.Errorf("duplicate: expected InvalidControlField, got %v", err)
	}
}

func TestRejectsWrongIncomingAddress(t *testing.T) {
	client := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleClient))
	server := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleServer))

	connReq, err := client.BuildConnectRequest()
	if err != nil {
		t.Fatal(err)
	}

	frame := decodeOrFail(t, connReq)
	wrongDst, err := hdlc.MakeClientAddress(0x10)
	if err != nil {
		t.Fatal(err)
	}
	frame.Destination = wrongDst

	if err := server.ReceiveFrame(frame); err != hdlc.StatusInvalidAddress {
		t.Errorf("expected InvalidAddress, got %v", err)
	}
}

func TestDisconnectExchangeReturnsBothEndpointsToDisconnected(t *testing.T) {
	client := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleClient))
	server := hdlc.NewSession(makeSessionOptions(t, hdlc.SessionRoleServer))
	establish(t, client, server)

	discReq, err := client.BuildDisconnectRequest()
	if err != nil {
		t.Fatal(err)
	}
	if client.State() != hdlc.SessionStateAwaitingDisconnect {
		t.Errorf("client state: got %v, want AwaitingDisconnect", client.State())
	}
	if err := server.ReceiveFrame(decodeOrFail(t, discReq)); err != nil {
		t.Fatalf("server.ReceiveFrame(DISC): %v", err)
	}

	discResp, err := server.BuildDisconnectResponse()
	if err != nil {
		t.Fatal(err)
	}
	if server.State() != hdlc.SessionStateDisconnected {
		t.Errorf("server state: got %v, want Disconnected", server.State())
	}
	if err := client.ReceiveFrame(decodeOrFail(t, discResp)); err != nil {
		t.Fatalf("client.ReceiveFrame(UA): %v", err)
	}
	if client.State() != hdlc.SessionStateDisconnected {
		t.Errorf("client state: got %v, want Disconnected", client.State())
	}
}
