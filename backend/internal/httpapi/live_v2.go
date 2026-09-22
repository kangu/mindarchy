package httpapi

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"net/http"
	"time"

	"github.com/coder/websocket"
	"mindarchy/backend/internal/auth"
	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
)

func (s *ProductionServer) liveV2Handler(w http.ResponseWriter, request *http.Request, mapID protocol.MapID, identity auth.Identity) {
	if s.rooms == nil {
		http.Error(w, "live protocol unavailable", http.StatusServiceUnavailable)
		return
	}
	room, err := s.rooms.OpenLive(request.Context(), mapID)
	if err != nil {
		http.Error(w, "could not open live map", http.StatusConflict)
		return
	}
	conn, err := websocket.Accept(w, request, &websocket.AcceptOptions{OriginPatterns: []string{}})
	if err != nil {
		return
	}
	defer conn.CloseNow()
	conn.SetReadLimit(2 << 20)
	controller := http.NewResponseController(w)
	_ = controller.SetReadDeadline(time.Time{})
	_ = controller.SetWriteDeadline(time.Time{})
	ctx, cancel := context.WithCancel(request.Context())
	defer cancel()
	peer := &livePeer{conn: conn}
	outgoing := make(chan rooms.LiveEvent, 32)
	enqueue := func(e rooms.LiveEvent) {
		select {
		case outgoing <- e:
		case <-ctx.Done():
		default:
			cancel()
			_ = conn.CloseNow()
		}
	}
	writerDone := make(chan struct{})
	helloWritten := make(chan struct{})
	go func() {
		defer close(writerDone)
		for {
			select {
			case <-ctx.Done():
				return
			case event := <-outgoing:
				writeCtx, stop := context.WithTimeout(ctx, 2*time.Second)
				err := peer.write(writeCtx, event)
				stop()
				if event.Type == "hello" && err == nil {
					close(helloWritten)
				}
				if err != nil {
					cancel()
					_ = conn.CloseNow()
					return
				}
			}
		}
	}()
	defer func() { cancel(); <-writerDone }()
	leave := room.Subscribe(identity.Account, enqueue)
	defer leave()
	select {
	case <-helloWritten:
	case <-ctx.Done():
		return
	}
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
	s.broadcastPeers(ctx, peers, map[string]any{"type": "presence", "accounts": roster})
	for {
		var frame struct {
			Type      string `json:"type"`
			Operation string `json:"operation"`
			Hash      string `json:"hash"`
		}
		_, body, err := conn.Read(ctx)
		if err != nil {
			return
		}
		// Strictly decode this bounded envelope; never pass legacy snapshots to v2.
		if json.Unmarshal(body, &frame) != nil {
			enqueue(rooms.LiveEvent{Type: "rejected", Code: protocol.ErrorInvalidMessage})
			continue
		}
		current, ok := s.identity(request)
		if !ok || current.Account != identity.Account {
			_ = peer.write(ctx, map[string]string{"type": "rejected", "code": protocol.ErrorAccessRevoked})
			return
		}
		role, err := s.sharing.Role(current.Account, mapID)
		if err != nil {
			_ = peer.write(ctx, map[string]string{"type": "rejected", "code": protocol.ErrorAccessRevoked})
			return
		}
		s.touchPresence(mapID, current.Account)
		if frame.Type == "presence" {
			continue
		}
		if frame.Type != "edit" {
			enqueue(rooms.LiveEvent{Type: "rejected", Code: "upgrade_required"})
			continue
		}
		if role == "viewer" {
			enqueue(rooms.LiveEvent{Type: "rejected", Code: protocol.ErrorAccessRevoked})
			continue
		}
		payload, err := base64.StdEncoding.DecodeString(frame.Operation)
		if err != nil || len(frame.Operation) > protocol.MaxEncodedChanges {
			enqueue(rooms.LiveEvent{Type: "rejected", Code: protocol.ErrorInvalidMessage})
			continue
		}
		if err := room.SubmitTo(ctx, current.Account, payload, frame.Hash, enqueue); err != nil {
			code := "storage_unavailable"
			var pe *protocol.ProtocolError
			if errors.As(err, &pe) {
				code = pe.Code
			} else if errors.Is(err, rooms.ErrAccessDenied) {
				code = protocol.ErrorAccessRevoked
			} else if errors.Is(err, rooms.ErrCounterReuse) {
				code = protocol.ErrorCounterReuse
			}
			var identity struct {
				ID string `json:"id"`
			}
			_ = json.Unmarshal(payload, &identity)
			enqueue(rooms.LiveEvent{Type: "rejected", Code: code, OpID: identity.ID})
		}
	}
}
