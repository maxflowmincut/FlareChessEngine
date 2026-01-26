package main

import (
	"bufio"
	"crypto/sha1"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

const (
	opText  = 1
	opClose = 8
	opPing  = 9
	opPong  = 10
)

type ClientMessage struct {
	Type       string `json:"type"`
	Uci        string `json:"uci,omitempty"`
	Color      string `json:"color,omitempty"`
	MovetimeMs int    `json:"movetime_ms,omitempty"`
}

type ServerMessage struct {
	Type       string   `json:"type"`
	Fen        string   `json:"fen,omitempty"`
	Moves      []string `json:"moves,omitempty"`
	EngineMove string   `json:"engine_move,omitempty"`
	Status     string   `json:"status,omitempty"`
	Message    string   `json:"message,omitempty"`
}

type WsConn struct {
	conn    net.Conn
	reader  *bufio.Reader
	writeMu sync.Mutex
}

func (c *WsConn) Close() error {
	return c.conn.Close()
}

func (c *WsConn) ReadMessage() ([]byte, error) {
	for {
		opcode, payload, err := readFrame(c.reader)
		if err != nil {
			return nil, err
		}
		switch opcode {
		case opText:
			return payload, nil
		case opPing:
			_ = c.writeFrame(opPong, payload)
		case opClose:
			return nil, io.EOF
		}
	}
}

func (c *WsConn) WriteMessage(payload []byte) error {
	return c.writeFrame(opText, payload)
}

func (c *WsConn) WriteJSON(value any) error {
	payload, err := json.Marshal(value)
	if err != nil {
		return err
	}
	return c.WriteMessage(payload)
}

func (c *WsConn) writeFrame(opcode byte, payload []byte) error {
	c.writeMu.Lock()
	defer c.writeMu.Unlock()

	header := make([]byte, 0, 14)
	header = append(header, 0x80|opcode)
	length := len(payload)
	switch {
	case length < 126:
		header = append(header, byte(length))
	case length <= 65535:
		header = append(header, 126)
		buf := make([]byte, 2)
		binary.BigEndian.PutUint16(buf, uint16(length))
		header = append(header, buf...)
	default:
		header = append(header, 127)
		buf := make([]byte, 8)
		binary.BigEndian.PutUint64(buf, uint64(length))
		header = append(header, buf...)
	}

	if _, err := c.conn.Write(header); err != nil {
		return err
	}
	_, err := c.conn.Write(payload)
	return err
}

func readFrame(reader *bufio.Reader) (byte, []byte, error) {
	b1, err := reader.ReadByte()
	if err != nil {
		return 0, nil, err
	}
	b2, err := reader.ReadByte()
	if err != nil {
		return 0, nil, err
	}
	fin := b1 & 0x80
	opcode := b1 & 0x0f
	masked := b2 & 0x80
	length := int(b2 & 0x7f)

	if fin == 0 {
		return 0, nil, errors.New("fragmented frames not supported")
	}

	if length == 126 {
		buf := make([]byte, 2)
		if _, err := io.ReadFull(reader, buf); err != nil {
			return 0, nil, err
		}
		length = int(binary.BigEndian.Uint16(buf))
	} else if length == 127 {
		buf := make([]byte, 8)
		if _, err := io.ReadFull(reader, buf); err != nil {
			return 0, nil, err
		}
		payloadLen := binary.BigEndian.Uint64(buf)
		if payloadLen > uint64(^uint(0)>>1) {
			return 0, nil, errors.New("payload too large")
		}
		length = int(payloadLen)
	}

	var maskKey [4]byte
	if masked != 0 {
		if _, err := io.ReadFull(reader, maskKey[:]); err != nil {
			return 0, nil, err
		}
	}

	payload := make([]byte, length)
	if _, err := io.ReadFull(reader, payload); err != nil {
		return 0, nil, err
	}

	if masked != 0 {
		for i := range payload {
			payload[i] ^= maskKey[i%4]
		}
	}

	return opcode, payload, nil
}

func upgradeToWebSocket(w http.ResponseWriter, r *http.Request) (*WsConn, error) {
	if !headerContains(r.Header, "Connection", "upgrade") ||
		!strings.EqualFold(r.Header.Get("Upgrade"), "websocket") {
		return nil, errors.New("not a websocket request")
	}

	key := r.Header.Get("Sec-WebSocket-Key")
	if key == "" {
		return nil, errors.New("missing websocket key")
	}

	accept := computeAcceptKey(key)
	if accept == "" {
		return nil, errors.New("invalid websocket key")
	}

	hijacker, ok := w.(http.Hijacker)
	if !ok {
		return nil, errors.New("hijacking not supported")
	}

	conn, _, err := hijacker.Hijack()
	if err != nil {
		return nil, err
	}

	response := "HTTP/1.1 101 Switching Protocols\r\n" +
		"Upgrade: websocket\r\n" +
		"Connection: Upgrade\r\n" +
		"Sec-WebSocket-Accept: " + accept + "\r\n\r\n"

	if _, err := conn.Write([]byte(response)); err != nil {
		_ = conn.Close()
		return nil, err
	}

	return &WsConn{conn: conn, reader: bufio.NewReader(conn)}, nil
}

func headerContains(header http.Header, key, value string) bool {
	for _, part := range header.Values(key) {
		for _, token := range strings.Split(part, ",") {
			if strings.EqualFold(strings.TrimSpace(token), value) {
				return true
			}
		}
	}
	return false
}

func computeAcceptKey(key string) string {
	const magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
	hash := sha1.Sum([]byte(key + magic))
	return base64.StdEncoding.EncodeToString(hash[:])
}

type EngineProcess struct {
	cmd    *exec.Cmd
	stdin  io.WriteCloser
	stdout *bufio.Reader
	mu     sync.Mutex
}

func startEngine(path string) (*EngineProcess, error) {
	if _, err := os.Stat(path); err != nil {
		return nil, fmt.Errorf("engine binary not found at %s", path)
	}
	cmd := exec.Command(path)
	stdin, err := cmd.StdinPipe()
	if err != nil {
		return nil, err
	}
	stdoutPipe, err := cmd.StdoutPipe()
	if err != nil {
		return nil, err
	}
	cmd.Stderr = cmd.Stdout
	if err := cmd.Start(); err != nil {
		return nil, err
	}
	engine := &EngineProcess{
		cmd:    cmd,
		stdin:  stdin,
		stdout: bufio.NewReader(stdoutPipe),
	}
	if err := engine.handshake(); err != nil {
		engine.Close()
		return nil, err
	}
	return engine, nil
}

func (e *EngineProcess) Close() {
	e.mu.Lock()
	defer e.mu.Unlock()
	_, _ = io.WriteString(e.stdin, "quit\n")
	_ = e.stdin.Close()
	_ = e.cmd.Wait()
}

func (e *EngineProcess) handshake() error {
	e.mu.Lock()
	defer e.mu.Unlock()
	if err := e.sendLocked("uci"); err != nil {
		return err
	}
	if _, err := e.waitForPrefixLocked("uciok"); err != nil {
		return err
	}
	if err := e.sendLocked("isready"); err != nil {
		return err
	}
	_, err := e.waitForPrefixLocked("readyok")
	return err
}

func (e *EngineProcess) NewGame() error {
	e.mu.Lock()
	defer e.mu.Unlock()
	return e.sendLocked("ucinewgame")
}

func (e *EngineProcess) LegalMoves(moves []string) ([]string, error) {
	e.mu.Lock()
	defer e.mu.Unlock()
	if err := e.sendLocked(buildPositionCommand(moves)); err != nil {
		return nil, err
	}
	if err := e.sendLocked("legalmoves"); err != nil {
		return nil, err
	}
	line, err := e.waitForPrefixLocked("legalmoves")
	if err != nil {
		return nil, err
	}
	fields := strings.Fields(line)
	if len(fields) <= 1 {
		return nil, nil
	}
	return fields[1:], nil
}

func (e *EngineProcess) BestMove(moves []string, depth int, movetimeMs int) (string, error) {
	e.mu.Lock()
	defer e.mu.Unlock()
	if err := e.sendLocked(buildPositionCommand(moves)); err != nil {
		return "", err
	}
	if movetimeMs > 0 {
		if err := e.sendLocked(fmt.Sprintf("go movetime %d", movetimeMs)); err != nil {
			return "", err
		}
	} else {
		if err := e.sendLocked(fmt.Sprintf("go depth %d", depth)); err != nil {
			return "", err
		}
	}
	for {
		line, err := e.readLineLocked()
		if err != nil {
			return "", err
		}
		if strings.HasPrefix(line, "bestmove ") {
			parts := strings.Fields(line)
			if len(parts) >= 2 {
				return parts[1], nil
			}
			return "", nil
		}
	}
}

func (e *EngineProcess) Fen(moves []string) (string, error) {
	e.mu.Lock()
	defer e.mu.Unlock()
	if err := e.sendLocked(buildPositionCommand(moves)); err != nil {
		return "", err
	}
	if err := e.sendLocked("fen"); err != nil {
		return "", err
	}
	line, err := e.waitForPrefixLocked("fen ")
	if err != nil {
		return "", err
	}
	return strings.TrimPrefix(line, "fen "), nil
}

func (e *EngineProcess) InCheck(moves []string) (bool, error) {
	e.mu.Lock()
	defer e.mu.Unlock()
	if err := e.sendLocked(buildPositionCommand(moves)); err != nil {
		return false, err
	}
	if err := e.sendLocked("incheck"); err != nil {
		return false, err
	}
	line, err := e.waitForPrefixLocked("incheck")
	if err != nil {
		return false, err
	}
	fields := strings.Fields(line)
	if len(fields) < 2 {
		return false, errors.New("invalid incheck response")
	}
	return fields[1] == "1", nil
}

func (e *EngineProcess) sendLocked(command string) error {
	_, err := io.WriteString(e.stdin, command+"\n")
	return err
}

func (e *EngineProcess) readLineLocked() (string, error) {
	line, err := e.stdout.ReadString('\n')
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(line), nil
}

func (e *EngineProcess) waitForPrefixLocked(prefix string) (string, error) {
	for {
		line, err := e.readLineLocked()
		if err != nil {
			return "", err
		}
		if strings.HasPrefix(line, prefix) {
			return line, nil
		}
	}
}

func buildPositionCommand(moves []string) string {
	if len(moves) == 0 {
		return "position startpos"
	}
	return "position startpos moves " + strings.Join(moves, " ")
}

type Session struct {
	engine        *EngineProcess
	moves         []string
	depth         int
	movetimeMs    int
	playerIsWhite bool
}

func sideToMoveIsWhite(moves []string) bool {
	return len(moves)%2 == 0
}

func sideToMoveIsPlayer(moves []string, playerIsWhite bool) bool {
	if playerIsWhite {
		return sideToMoveIsWhite(moves)
	}
	return !sideToMoveIsWhite(moves)
}

func gameOverMessage(moves []string, playerIsWhite bool, inCheck bool) string {
	if !inCheck {
		return "Stalemate. Draw."
	}
	if sideToMoveIsPlayer(moves, playerIsWhite) {
		return "Checkmate. Engine wins."
	}
	return "Checkmate. You win."
}

func parsePlayerColor(value string, fallback bool) bool {
	if value == "" {
		return fallback
	}
	return !strings.EqualFold(strings.TrimSpace(value), "black")
}

func (s *Session) Reset(playerIsWhite bool) (string, string, string, error) {
	s.moves = nil
	s.playerIsWhite = playerIsWhite
	if err := s.engine.NewGame(); err != nil {
		return "", "", "", err
	}
	if playerIsWhite {
		return "Your move", "", "", nil
	}

	engineMoves, err := s.engine.LegalMoves(s.moves)
	if err != nil {
		return "", "", "", err
	}
	if len(engineMoves) == 0 {
		inCheck, err := s.engine.InCheck(s.moves)
		if err != nil {
			return "", "", "", err
		}
		return "Game over", gameOverMessage(s.moves, s.playerIsWhite, inCheck), "", nil
	}

	bestMove, err := s.engine.BestMove(s.moves, s.depth, s.movetimeMs)
	if err != nil {
		return "", "", "", err
	}
	if bestMove == "" || bestMove == "(none)" || bestMove == "0000" {
		inCheck, err := s.engine.InCheck(s.moves)
		if err != nil {
			return "", "", "", err
		}
		return "Game over", gameOverMessage(s.moves, s.playerIsWhite, inCheck), "", nil
	}
	s.moves = append(s.moves, bestMove)

	playerMoves, err := s.engine.LegalMoves(s.moves)
	if err != nil {
		return "", "", "", err
	}
	if len(playerMoves) == 0 {
		inCheck, err := s.engine.InCheck(s.moves)
		if err != nil {
			return "", "", "", err
		}
		return "Game over", gameOverMessage(s.moves, s.playerIsWhite, inCheck), bestMove, nil
	}
	return "Your move", "", bestMove, nil
}

func (s *Session) SendState(ws *WsConn, status, engineMove, message string) error {
	fen, err := s.engine.Fen(s.moves)
	if err != nil {
		return err
	}
	state := ServerMessage{
		Type:       "state",
		Fen:        fen,
		Moves:      append([]string(nil), s.moves...),
		EngineMove: engineMove,
		Status:     status,
		Message:    message,
	}
	return ws.WriteJSON(state)
}

func handleSession(ws *WsConn, enginePath string, depth int, movetimeMs int) {
	defer ws.Close()

	engine, err := startEngine(enginePath)
	if err != nil {
		_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
		return
	}
	defer engine.Close()

	session := &Session{
		engine:        engine,
		depth:         depth,
		movetimeMs:    movetimeMs,
		playerIsWhite: true,
	}
	status, message, engineMove, err := session.Reset(true)
	if err != nil {
		_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
		return
	}

	if err := session.SendState(ws, status, engineMove, message); err != nil {
		_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
		return
	}

	for {
		payload, err := ws.ReadMessage()
		if err != nil {
			return
		}
		var msg ClientMessage
		if err := json.Unmarshal(payload, &msg); err != nil {
			_ = ws.WriteJSON(ServerMessage{Type: "error", Message: "invalid json"})
			continue
		}
		switch msg.Type {
		case "new":
			playerIsWhite := parsePlayerColor(msg.Color, session.playerIsWhite)
			status, message, engineMove, err := session.Reset(playerIsWhite)
			if err != nil {
				_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
				continue
			}
			_ = session.SendState(ws, status, engineMove, message)
		case "movetime":
			value := msg.MovetimeMs
			if value < 0 {
				value = 0
			}
			if value > 10000 {
				value = 10000
			}
			session.movetimeMs = value
		case "move":
			uci := strings.TrimSpace(msg.Uci)
			if uci == "" {
				_ = ws.WriteJSON(ServerMessage{Type: "error", Message: "missing move"})
				continue
			}
			legalMoves, err := session.engine.LegalMoves(session.moves)
			if err != nil {
				_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
				continue
			}
			if !containsMove(legalMoves, uci) {
				_ = ws.WriteJSON(ServerMessage{Type: "error", Message: "illegal move"})
				continue
			}
			session.moves = append(session.moves, uci)
			engineMoves, err := session.engine.LegalMoves(session.moves)
			if err != nil {
				_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
				continue
			}
			if len(engineMoves) == 0 {
				inCheck, err := session.engine.InCheck(session.moves)
				if err != nil {
					_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
					continue
				}
				message := gameOverMessage(session.moves, session.playerIsWhite, inCheck)
				_ = session.SendState(ws, "Game over", "", message)
				continue
			}

			if err := session.SendState(ws, "Engine thinking", "", ""); err != nil {
				_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
				continue
			}
			bestMove, err := session.engine.BestMove(session.moves, session.depth, session.movetimeMs)
			if err != nil {
				_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
				continue
			}
			status := "Your move"
			message := ""
			if bestMove == "" || bestMove == "(none)" || bestMove == "0000" {
				inCheck, err := session.engine.InCheck(session.moves)
				if err != nil {
					_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
					continue
				}
				status = "Game over"
				message = gameOverMessage(session.moves, session.playerIsWhite, inCheck)
			} else {
				session.moves = append(session.moves, bestMove)
				playerMoves, err := session.engine.LegalMoves(session.moves)
				if err != nil {
					_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
					continue
				}
				if len(playerMoves) == 0 {
					inCheck, err := session.engine.InCheck(session.moves)
					if err != nil {
						_ = ws.WriteJSON(ServerMessage{Type: "error", Message: err.Error()})
						continue
					}
					status = "Game over"
					message = gameOverMessage(session.moves, session.playerIsWhite, inCheck)
				}
			}
			_ = session.SendState(ws, status, bestMove, message)
		default:
			_ = ws.WriteJSON(ServerMessage{Type: "error", Message: "unknown command"})
		}
	}
}

func containsMove(moves []string, target string) bool {
	for _, move := range moves {
		if move == target {
			return true
		}
	}
	return false
}

func defaultEnginePath() string {
	cwd, err := os.Getwd()
	if err != nil {
		return "build/engine/flare_engine"
	}
	if filepath.Base(cwd) == "web" {
		return filepath.Join(cwd, "..", "build", "engine", "flare_engine")
	}
	return filepath.Join(cwd, "build", "engine", "flare_engine")
}

func main() {
	addr := flag.String("addr", "127.0.0.1:8080", "listen address")
	enginePath := flag.String("engine", defaultEnginePath(), "path to engine binary")
	depth := flag.Int("depth", 4, "search depth for engine replies")
	movetimeMs := flag.Int("movetime", 1000, "search time per move in ms (overrides depth when > 0)")
	staticDir := flag.String("static", "static", "static file directory")
	flag.Parse()

	mux := http.NewServeMux()
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, _ *http.Request) {
		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte("ok"))
	})
	mux.Handle("/", http.FileServer(http.Dir(*staticDir)))
	mux.HandleFunc("/ws", func(w http.ResponseWriter, r *http.Request) {
		ws, err := upgradeToWebSocket(w, r)
		if err != nil {
			http.Error(w, "websocket upgrade failed", http.StatusBadRequest)
			return
		}
		go handleSession(ws, *enginePath, *depth, *movetimeMs)
	})

	log.Printf("listening on http://%s", *addr)
	log.Printf("engine path %s", *enginePath)
	server := &http.Server{
		Addr:              *addr,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
		IdleTimeout:       60 * time.Second,
	}
	if err := server.ListenAndServe(); err != nil {
		log.Fatal(err)
	}
}
