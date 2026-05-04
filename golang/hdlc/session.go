package hdlc

// SessionRole is the endpoint role of the HDLC session state machine.
type SessionRole int

const (
	// SessionRoleClient initiates SNRM and DISC.
	SessionRoleClient SessionRole = iota
	// SessionRoleServer accepts SNRM and DISC.
	SessionRoleServer
)

// SessionState is the current HDLC session state.
type SessionState int

const (
	// SessionStateDisconnected means no active HDLC data-link session.
	SessionStateDisconnected SessionState = iota
	// SessionStateAwaitingConnection means the client sent SNRM and waits for UA.
	SessionStateAwaitingConnection
	// SessionStateConnected means the session is established and I/S frames may flow.
	SessionStateConnected
	// SessionStateAwaitingDisconnect means the endpoint sent DISC and waits for UA.
	SessionStateAwaitingDisconnect
)

// SessionOptions is the fixed configuration for one HDLC session endpoint.
type SessionOptions struct {
	Role          SessionRole
	ClientAddress Address
	ServerAddress Address
	Limits        CodecLimits
}

// Raw control byte constants for session-layer unnumbered frames.
const (
	ctrlSNRM            = byte(0x93)
	ctrlUA              = byte(0x73)
	ctrlDISC            = byte(0x53)
	ctrlReceiveReadyBase = byte(0x01)
)

func nextSequence(seq uint8) uint8 {
	return (seq + 1) & 0x07
}

func makeInformationControl(ns, nr uint8, pollFinal bool) byte {
	ctrl := ((ns & 0x07) << 1) | ((nr & 0x07) << 5)
	if pollFinal {
		ctrl |= 0x10
	}
	return ctrl
}

func makeReceiveReadyControl(nr uint8, pollFinal bool) byte {
	ctrl := ctrlReceiveReadyBase | ((nr & 0x07) << 5)
	if pollFinal {
		ctrl |= 0x10
	}
	return ctrl
}

// Session is a minimal HDLC session state machine built on top of the frame codec.
// It validates endpoint addresses, performs SNRM/UA and DISC/UA state transitions,
// and tracks three-bit I-frame send/receive sequence numbers.
// It does not own transport I/O, timers, retransmission, or APDU/LLC parsing.
type Session struct {
	options                   SessionOptions
	state                     SessionState
	sendSequence              uint8
	receiveSequence           uint8
	connectResponsePending    bool
	disconnectResponsePending bool
}

// NewSession creates a session endpoint with the given configuration.
func NewSession(opts SessionOptions) *Session {
	return &Session{options: opts, state: SessionStateDisconnected}
}

// State returns the current session state.
func (s *Session) State() SessionState { return s.state }

// SendSequence returns the next local I-frame send sequence number.
func (s *Session) SendSequence() uint8 { return s.sendSequence }

// ReceiveSequence returns the next expected remote I-frame send sequence number.
func (s *Session) ReceiveSequence() uint8 { return s.receiveSequence }

// BuildConnectRequest builds a client SNRM request and enters AwaitingConnection.
// Returns UnsupportedFeature for the server role.
func (s *Session) BuildConnectRequest() ([]byte, error) {
	if s.options.Role != SessionRoleClient {
		return nil, StatusUnsupportedFeature
	}
	if s.state != SessionStateDisconnected {
		return nil, StatusUnsupportedFrame
	}
	out, err := s.buildUnnumberedFrame(ctrlSNRM)
	if err != nil {
		return nil, err
	}
	s.state = SessionStateAwaitingConnection
	s.sendSequence = 0
	s.receiveSequence = 0
	return out, nil
}

// BuildConnectResponse builds a server UA response after receiving SNRM.
// Returns UnsupportedFeature for the client role.
func (s *Session) BuildConnectResponse() ([]byte, error) {
	if s.options.Role != SessionRoleServer {
		return nil, StatusUnsupportedFeature
	}
	if !s.connectResponsePending {
		return nil, StatusUnsupportedFrame
	}
	out, err := s.buildUnnumberedFrame(ctrlUA)
	if err != nil {
		return nil, err
	}
	s.state = SessionStateConnected
	s.connectResponsePending = false
	s.sendSequence = 0
	s.receiveSequence = 0
	return out, nil
}

// BuildDisconnectRequest builds a DISC request and enters AwaitingDisconnect.
func (s *Session) BuildDisconnectRequest() ([]byte, error) {
	if s.state != SessionStateConnected {
		return nil, StatusUnsupportedFrame
	}
	out, err := s.buildUnnumberedFrame(ctrlDISC)
	if err != nil {
		return nil, err
	}
	s.state = SessionStateAwaitingDisconnect
	return out, nil
}

// BuildDisconnectResponse builds a UA response after receiving DISC and enters Disconnected.
func (s *Session) BuildDisconnectResponse() ([]byte, error) {
	if !s.disconnectResponsePending {
		return nil, StatusUnsupportedFrame
	}
	out, err := s.buildUnnumberedFrame(ctrlUA)
	if err != nil {
		return nil, err
	}
	s.state = SessionStateDisconnected
	s.disconnectResponsePending = false
	s.sendSequence = 0
	s.receiveSequence = 0
	return out, nil
}

// BuildInformationFrame builds one I-frame and advances the local send sequence on success.
func (s *Session) BuildInformationFrame(information []byte, pollFinal bool) ([]byte, error) {
	if s.state != SessionStateConnected {
		return nil, StatusUnsupportedFrame
	}
	ctrl := makeInformationControl(s.sendSequence, s.receiveSequence, pollFinal)
	out, err := s.buildFrame(ctrl, information)
	if err != nil {
		return nil, err
	}
	s.sendSequence = nextSequence(s.sendSequence)
	return out, nil
}

// BuildReceiveReadyFrame builds a Receive Ready supervisory frame.
func (s *Session) BuildReceiveReadyFrame(pollFinal bool) ([]byte, error) {
	if s.state != SessionStateConnected {
		return nil, StatusUnsupportedFrame
	}
	ctrl := makeReceiveReadyControl(s.receiveSequence, pollFinal)
	return s.buildFrame(ctrl, nil)
}

// ReceiveFrame consumes an incoming decoded frame and updates session state.
func (s *Session) ReceiveFrame(frame Frame) error {
	if !s.incomingAddressesMatch(frame) {
		return StatusInvalidAddress
	}
	switch frame.Control.FrameKind() {
	case FrameKindInformation:
		return s.receiveInformationFrame(frame)
	case FrameKindSupervisory:
		return s.receiveSupervisoryFrame(frame)
	case FrameKindUnnumbered:
		return s.receiveUnnumberedFrame(frame)
	default:
		return StatusInvalidFrameType
	}
}

func (s *Session) buildUnnumberedFrame(ctrl byte) ([]byte, error) {
	return s.buildFrame(ctrl, nil)
}

func (s *Session) buildFrame(ctrl byte, information []byte) ([]byte, error) {
	frame := Frame{
		Segmented:   false,
		Destination: s.remoteAddress(),
		Source:      s.localAddress(),
		Control:     ControlFromRaw(ctrl),
		Information: information,
	}
	return EncodeFrame(frame, s.options.Limits)
}

func (s *Session) receiveInformationFrame(frame Frame) error {
	if s.state != SessionStateConnected {
		return StatusUnsupportedFrame
	}
	if frame.Control.SendSequence() != s.receiveSequence {
		return StatusInvalidControlField
	}
	if frame.Control.ReceiveSequence() != s.sendSequence {
		return StatusInvalidControlField
	}
	s.receiveSequence = nextSequence(s.receiveSequence)
	return nil
}

func (s *Session) receiveSupervisoryFrame(frame Frame) error {
	if s.state != SessionStateConnected {
		return StatusUnsupportedFrame
	}
	if _, err := frame.Control.SupervisoryKind(); err != nil {
		return err
	}
	if frame.Control.ReceiveSequence() != s.sendSequence {
		return StatusInvalidControlField
	}
	return nil
}

func (s *Session) receiveUnnumberedFrame(frame Frame) error {
	kind, err := frame.Control.UnnumberedKind()
	if err != nil {
		return err
	}

	switch kind {
	case UnnumberedSnrm:
		if s.options.Role != SessionRoleServer || s.state != SessionStateDisconnected {
			return StatusUnsupportedFrame
		}
		s.connectResponsePending = true
		s.sendSequence = 0
		s.receiveSequence = 0
		return nil

	case UnnumberedUa:
		if s.state == SessionStateAwaitingConnection {
			s.state = SessionStateConnected
			s.sendSequence = 0
			s.receiveSequence = 0
			return nil
		}
		if s.state == SessionStateAwaitingDisconnect {
			s.state = SessionStateDisconnected
			s.sendSequence = 0
			s.receiveSequence = 0
			return nil
		}
		return StatusUnsupportedFrame

	case UnnumberedDisc:
		if s.state != SessionStateConnected {
			return StatusUnsupportedFrame
		}
		s.disconnectResponsePending = true
		return nil

	case UnnumberedDm:
		s.state = SessionStateDisconnected
		s.sendSequence = 0
		s.receiveSequence = 0
		return nil
	}

	return StatusUnsupportedFrame
}

func (s *Session) incomingAddressesMatch(frame Frame) bool {
	return sameAddress(frame.Destination, s.localAddress()) &&
		sameAddress(frame.Source, s.remoteAddress())
}

func (s *Session) localAddress() Address {
	if s.options.Role == SessionRoleClient {
		return s.options.ClientAddress
	}
	return s.options.ServerAddress
}

func (s *Session) remoteAddress() Address {
	if s.options.Role == SessionRoleClient {
		return s.options.ServerAddress
	}
	return s.options.ClientAddress
}
