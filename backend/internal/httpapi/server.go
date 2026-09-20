package httpapi

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
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

type ProductionServer struct {
	ready   func(context.Context) error
	verify  *auth.Verifier
	sharing *sharing.Service
	session *auth.CouchSession
	rooms   *rooms.Manager
	liveMu  sync.Mutex
	live    map[protocol.MapID]map[*livePeer]struct{}
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

func NewProductionServer(ready func(context.Context) error, verify *auth.Verifier, service *sharing.Service, session *auth.CouchSession, manager *rooms.Manager) *ProductionServer {
	return &ProductionServer{ready: ready, verify: verify, sharing: service, session: session, rooms: manager, live: map[protocol.MapID]map[*livePeer]struct{}{}}
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
	body, err := io.ReadAll(http.MaxBytesReader(w, request.Body, 20<<20))
	if err != nil {
		http.Error(w, "body too large", http.StatusRequestEntityTooLarge)
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
	peer := &livePeer{conn: conn}
	s.liveMu.Lock()
	if s.live[mapID] == nil {
		s.live[mapID] = map[*livePeer]struct{}{}
	}
	s.live[mapID][peer] = struct{}{}
	s.liveMu.Unlock()
	defer func() { s.liveMu.Lock(); delete(s.live[mapID], peer); s.liveMu.Unlock() }()
	_ = peer.write(request.Context(), map[string]any{"type": "hello", "mapId": mapID, "role": role})
	for {
		var message struct {
			Type    string          `json:"type"`
			Changes json.RawMessage `json:"changes"`
		}
		if err := wsjson.Read(request.Context(), conn, &message); err != nil {
			return
		}
		switch message.Type {
		case "presence":
			s.broadcast(request.Context(), mapID, map[string]any{"type": "presence", "accountId": identity.Account})
		case "submit":
			if role == "viewer" {
				_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": "access_revoked"})
				continue
			}
			if s.rooms == nil {
				_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": "resync_required"})
				continue
			}
			update, err := protocol.DecodeSubmit(message.Changes)
			if err != nil {
				_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": "invalid_message"})
				continue
			}
			if update.MapID != mapID {
				_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": "access_revoked"})
				continue
			}
			receipt, err := s.rooms.Submit(request.Context(), identity.Account, update)
			if err != nil {
				_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": err.Error()})
				continue
			}
			s.broadcast(request.Context(), mapID, map[string]any{"type": "committed", "receipt": receipt})
		default:
			_ = peer.write(request.Context(), map[string]string{"type": "rejected", "code": "invalid_message"})
		}
	}
}

func (s *ProductionServer) broadcast(ctx context.Context, mapID protocol.MapID, value any) {
	s.liveMu.Lock()
	peers := make([]*livePeer, 0, len(s.live[mapID]))
	for peer := range s.live[mapID] {
		peers = append(peers, peer)
	}
	s.liveMu.Unlock()
	for _, peer := range peers {
		_ = peer.write(ctx, value)
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
