package httpapi

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/coder/websocket"
	"github.com/coder/websocket/wsjson"
	"mindarchy/backend/internal/auth"
	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
)

const presenceDefaultTTL = 10 * time.Second

type ProductionServer struct {
	ready             func(context.Context) error
	verify            *auth.Verifier
	sharing           *sharing.Service
	session           *auth.CouchSession
	rooms             *rooms.Manager
	presenceTTLConfig time.Duration
	liveMu            sync.Mutex
	live              map[protocol.MapID]map[*livePeer]struct{}
	presence          map[protocol.MapID]map[protocol.AccountID]time.Time
	janitorStop       chan struct{}
	janitorDone       chan struct{}
	closeOnce         sync.Once
}

type ServerOption func(*ProductionServer)

func WithPresenceTTL(ttl time.Duration) ServerOption {
	return func(server *ProductionServer) { server.presenceTTLConfig = ttl }
}

type livePeer struct {
	conn *websocket.Conn
	mu   sync.Mutex
}

func (p *livePeer) write(ctx context.Context, value any) error {
	p.mu.Lock()
	defer p.mu.Unlock()
	return wsjson.Write(ctx, p.conn, value)
}

func NewProductionServer(ready func(context.Context) error, verify *auth.Verifier, service *sharing.Service, session *auth.CouchSession, manager *rooms.Manager, options ...ServerOption) *ProductionServer {
	server := &ProductionServer{ready: ready, verify: verify, sharing: service, session: session, rooms: manager, live: map[protocol.MapID]map[*livePeer]struct{}{}, presence: map[protocol.MapID]map[protocol.AccountID]time.Time{}, janitorStop: make(chan struct{}), janitorDone: make(chan struct{})}
	for _, option := range options {
		option(server)
	}
	if server.rooms != nil && service != nil {
		server.rooms.CheckTarget = func(_ context.Context, mapID protocol.MapID, account protocol.AccountID) error {
			role, err := service.Role(account, mapID)
			if err != nil || role == "viewer" {
				return rooms.ErrAccessDenied
			}
			return nil
		}
	}
	go server.presenceJanitor()
	return server
}

func (s *ProductionServer) presenceInterval() time.Duration {
	interval := s.presenceTTL() / 10
	if interval < 50*time.Millisecond {
		interval = 50 * time.Millisecond
	}
	return interval
}

func (s *ProductionServer) presenceTTL() time.Duration {
	if s.presenceTTLConfig > 0 {
		return s.presenceTTLConfig
	}
	return presenceDefaultTTL
}

func (s *ProductionServer) Close() {
	s.closeOnce.Do(func() {
		close(s.janitorStop)
		<-s.janitorDone
	})
}

func (s *ProductionServer) presenceJanitor() {
	defer close(s.janitorDone)
	ticker := time.NewTicker(s.presenceInterval())
	defer ticker.Stop()
	for {
		select {
		case <-s.janitorStop:
			return
		case <-ticker.C:
		}
		type sweep struct {
			mapID  protocol.MapID
			peers  []*livePeer
			roster []string
		}
		var sweeps []sweep
		s.liveMu.Lock()
		for mapID, entries := range s.presence {
			changed := false
			for account, lastSeen := range entries {
				if time.Since(lastSeen) > s.presenceTTL() {
					delete(entries, account)
					changed = true
				}
			}
			if len(entries) == 0 {
				delete(s.presence, mapID)
			}
			if changed && len(s.live[mapID]) > 0 {
				sweeps = append(sweeps, sweep{mapID: mapID, peers: s.peersLocked(mapID), roster: s.rosterLocked(mapID)})
			}
		}
		s.liveMu.Unlock()
		for _, item := range sweeps {
			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			for _, peer := range item.peers {
				_ = peer.write(ctx, map[string]any{"type": "presence", "accounts": item.roster})
			}
			cancel()
		}
	}
}

func (s *ProductionServer) peersLocked(mapID protocol.MapID) []*livePeer {
	peers := make([]*livePeer, 0, len(s.live[mapID]))
	for peer := range s.live[mapID] {
		peers = append(peers, peer)
	}
	return peers
}

func (s *ProductionServer) rosterLocked(mapID protocol.MapID) []string {
	accounts := make([]string, 0, len(s.presence[mapID]))
	for account := range s.presence[mapID] {
		accounts = append(accounts, string(account))
	}
	sort.Strings(accounts)
	return accounts
}

func (s *ProductionServer) touchPresence(mapID protocol.MapID, account protocol.AccountID) {
	s.liveMu.Lock()
	if s.presence[mapID] == nil {
		s.presence[mapID] = map[protocol.AccountID]time.Time{}
	}
	_, existed := s.presence[mapID][account]
	s.presence[mapID][account] = time.Now()
	if existed {
		s.liveMu.Unlock()
		return
	}
	roster := s.rosterLocked(mapID)
	peers := s.peersLocked(mapID)
	s.liveMu.Unlock()
	s.broadcastPeers(context.Background(), peers, map[string]any{"type": "presence", "accounts": roster})
}

func (s *ProductionServer) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, _ *http.Request) {
		writeJSON(w, http.StatusOK, map[string]string{"status": "alive"})
	})
	mux.HandleFunc("/readyz", s.readyHandler)
	mux.HandleFunc("/v1/me", s.meHandler)
	mux.HandleFunc("/v1/auth/session", s.sessionLogin)
	mux.HandleFunc("/v1/maps", s.mapsHandler)
	mux.HandleFunc("/v1/maps/", s.mapHandler)
	mux.HandleFunc("/v1/invites/", s.acceptInvite)
	return mux
}

func (s *ProductionServer) identity(request *http.Request) (auth.Identity, bool) {
	if s.verify != nil {
		if identity, err := s.verify.VerifyRequest(request.Context(), request); err == nil {
			return identity, true
		}
	}
	if s.session != nil {
		if identity, err := s.session.Verify(request.Context(), request); err == nil {
			return identity, true
		}
	}
	return auth.Identity{}, false
}
func (s *ProductionServer) mapsHandler(w http.ResponseWriter, request *http.Request) {
	identity, ok := s.identity(request)
	if !ok {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}
	if request.Method == http.MethodGet {
		writeJSON(w, http.StatusOK, s.sharing.List(identity.Account))
		return
	}
	if request.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	body, err := io.ReadAll(http.MaxBytesReader(w, request.Body, 1<<20))
	if err != nil || !json.Valid(body) {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": protocol.ErrorInvalidMessage})
		return
	}
	entry := s.sharing.Create(identity.Account, body)
	if err := s.sharing.PersistMap(entry); err != nil {
		http.Error(w, "map persistence failed", http.StatusServiceUnavailable)
		return
	}
	writeJSON(w, http.StatusCreated, map[string]any{"id": entry.ID, "role": "owner"})
}

func (s *ProductionServer) mapHandler(w http.ResponseWriter, request *http.Request) {
	identity, ok := s.identity(request)
	if !ok {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}
	parts := strings.Split(strings.Trim(request.URL.Path, "/"), "/")
	if len(parts) < 3 {
		http.NotFound(w, request)
		return
	}
	if len(parts) == 4 && parts[3] == "live" {
		s.liveHandler(w, request, protocol.MapID(parts[2]))
		return
	}
	entry, err := s.sharing.Get(identity.Account, protocol.MapID(parts[2]))
	if err != nil {
		http.NotFound(w, request)
		return
	}
	if request.Method == http.MethodGet {
		writeJSON(w, http.StatusOK, entry)
		return
	}
	if len(parts) == 4 && parts[3] == "invites" && request.Method == http.MethodPost {
		var body struct {
			Account string `json:"account"`
			Role    string `json:"role"`
		}
		if json.NewDecoder(request.Body).Decode(&body) != nil {
			http.Error(w, "invalid body", 400)
			return
		}
		token, err := s.sharing.Invite(identity.Account, entry.ID, protocol.AccountID(body.Account), body.Role)
		if err != nil {
			http.Error(w, "forbidden", 403)
			return
		}
		writeJSON(w, http.StatusCreated, map[string]string{"token": token})
		return
	}
	http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
}

func (s *ProductionServer) liveHandler(w http.ResponseWriter, request *http.Request, mapID protocol.MapID) {
	identity, ok := s.identity(request)
	if !ok {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}
	role, err := s.sharing.Role(identity.Account, mapID)
	if err != nil {
		http.NotFound(w, request)
		return
	}
	conn, err := websocket.Accept(w, request, &websocket.AcceptOptions{OriginPatterns: []string{}})
	if err != nil {
		return
	}
	defer conn.Close(websocket.StatusNormalClosure, "")
	controller := http.NewResponseController(w)
	_ = controller.SetReadDeadline(time.Time{})
	_ = controller.SetWriteDeadline(time.Time{})
	peer := &livePeer{conn: conn}
	s.liveMu.Lock()
	if s.live[mapID] == nil {
		s.live[mapID] = map[*livePeer]struct{}{}
	}
	s.live[mapID][peer] = struct{}{}
	if s.presence[mapID] == nil {
		s.presence[mapID] = map[protocol.AccountID]time.Time{}
	}
	s.presence[mapID][identity.Account] = time.Now()
	roster := s.rosterLocked(mapID)
	peers := s.peersLocked(mapID)
	s.liveMu.Unlock()
	defer func() {
		s.liveMu.Lock()
		delete(s.live[mapID], peer)
		if len(s.live[mapID]) == 0 {
			delete(s.live, mapID)
		}
		s.liveMu.Unlock()
	}()
	_ = peer.write(request.Context(), map[string]any{"type": "hello", "mapId": mapID, "role": role})
	ctx, cancel := context.WithTimeout(request.Context(), 5*time.Second)
	for _, target := range peers {
		_ = target.write(ctx, map[string]any{"type": "presence", "accounts": roster})
	}
	cancel()
	for {
		identity, ok := s.identity(request)
		if !ok {
			_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": protocol.ErrorAccessRevoked})
			return
		}
		var message struct {
			Type    string          `json:"type"`
			Changes json.RawMessage `json:"changes"`
		}
		if err := wsjson.Read(request.Context(), conn, &message); err != nil {
			return
		}
		s.touchPresence(mapID, identity.Account)
		switch message.Type {
		case "presence":
		case "submit":
			role, err := s.sharing.Role(identity.Account, mapID)
			if err != nil || role == "viewer" {
				_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": protocol.ErrorAccessRevoked})
				return
			}
			if s.rooms == nil {
				_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": protocol.ErrorResync})
				continue
			}
			update, err := protocol.DecodeSubmit(message.Changes)
			if err != nil {
				code := protocol.ErrorInvalidMessage
				var protocolErr *protocol.ProtocolError
				if errors.As(err, &protocolErr) {
					code = protocolErr.Code
				}
				_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": code})
				continue
			}
			if update.MapID != mapID {
				_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": protocol.ErrorAccessRevoked})
				continue
			}
			receipt, err := s.rooms.Submit(request.Context(), identity.Account, update)
			if err != nil {
				_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": submitRejectionCode(err)})
				if errors.Is(err, rooms.ErrAccessDenied) {
					return
				}
				continue
			}
			state := base64.StdEncoding.EncodeToString(update.Changes)
			s.broadcast(request.Context(), mapID, map[string]any{"type": "committed", "receipt": receipt, "state": state, "sender": string(identity.Account)})
		default:
			_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": protocol.ErrorInvalidMessage})
		}
	}
}

func submitRejectionCode(err error) string {
	switch {
	case errors.Is(err, rooms.ErrAccessDenied):
		return protocol.ErrorAccessRevoked
	case errors.Is(err, rooms.ErrInvalidMessage), errors.Is(err, rooms.ErrInvalidSnapshot):
		return protocol.ErrorInvalidMessage
	case errors.Is(err, rooms.ErrCounterReuse):
		return protocol.ErrorCounterReuse
	}
	return protocol.ErrorResync
}

func (s *ProductionServer) broadcast(ctx context.Context, mapID protocol.MapID, value any) {
	s.liveMu.Lock()
	peers := make([]*livePeer, 0, len(s.live[mapID]))
	for peer := range s.live[mapID] {
		peers = append(peers, peer)
	}
	s.liveMu.Unlock()
	s.broadcastPeers(ctx, peers, value)
}

func (s *ProductionServer) broadcastPeers(ctx context.Context, peers []*livePeer, value any) {
	for _, peer := range peers {
		writeCtx, cancel := context.WithTimeout(ctx, 2*time.Second)
		_ = peer.write(writeCtx, value)
		cancel()
	}
}

func (s *ProductionServer) acceptInvite(w http.ResponseWriter, request *http.Request) {
	identity, ok := s.identity(request)
	if !ok {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}
	token := strings.TrimPrefix(strings.Trim(request.URL.Path, "/"), "v1/invites/")
	token = strings.TrimSuffix(token, "/accept")
	entry, err := s.sharing.Accept(identity.Account, token)
	if err != nil {
		http.NotFound(w, request)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"id": entry.ID, "role": entry.ACL[identity.Account]})
}

func (s *ProductionServer) meHandler(w http.ResponseWriter, request *http.Request) {
	identity, ok := s.identity(request)
	if !ok {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}
	writeJSON(w, http.StatusOK, map[string]string{"accountId": string(identity.Account), "issuer": identity.Issuer, "subject": identity.Subject})
}

func (s *ProductionServer) sessionLogin(w http.ResponseWriter, request *http.Request) {
	if s.session == nil {
		http.Error(w, "session authentication unavailable", http.StatusServiceUnavailable)
		return
	}
	var credentials struct {
		Username string `json:"username"`
		Password string `json:"password"`
	}
	if json.NewDecoder(http.MaxBytesReader(w, request.Body, 8<<10)).Decode(&credentials) != nil || credentials.Username == "" || credentials.Password == "" {
		http.Error(w, "invalid credentials", http.StatusBadRequest)
		return
	}
	identity, value, err := s.session.Login(request.Context(), credentials.Username, credentials.Password)
	if err != nil {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}
	http.SetCookie(w, &http.Cookie{Name: "AuthSession", Value: value, Path: "/", HttpOnly: true, Secure: request.TLS != nil, SameSite: http.SameSiteLaxMode})
	writeJSON(w, http.StatusOK, map[string]string{"accountId": string(identity.Account)})
}

func (s *ProductionServer) readyHandler(w http.ResponseWriter, request *http.Request) {
	if s.ready != nil {
		ctx, cancel := context.WithTimeout(request.Context(), 2*time.Second)
		defer cancel()
		if err := s.ready(ctx); err != nil {
			http.Error(w, "not ready", http.StatusServiceUnavailable)
			return
		}
	}
	writeJSON(w, http.StatusOK, map[string]string{"status": "ready"})
}
