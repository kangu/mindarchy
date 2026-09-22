package httpapi

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"

	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
)

type TestServer struct {
	rooms map[protocol.MapID]*rooms.Room
}

func NewTestServer(mapID protocol.MapID) *TestServer {
	return &TestServer{rooms: map[protocol.MapID]*rooms.Room{mapID: rooms.New(mapID)}}
}

func (s *TestServer) Handler() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/v1/test/maps/", s.handleMap)
	return mux
}

func (s *TestServer) handleMap(w http.ResponseWriter, request *http.Request) {
	parts := strings.Split(strings.Trim(request.URL.Path, "/"), "/")
	if len(parts) != 5 || parts[0] != "v1" || parts[1] != "test" || parts[2] != "maps" || parts[4] != "changes" {
		http.NotFound(w, request)
		return
	}
	mapID := protocol.MapID(parts[3])
	room, ok := s.rooms[mapID]
	if !ok {
		http.NotFound(w, request)
		return
	}
	account := protocol.AccountID(request.Header.Get("X-Test-Account"))
	if account == "" {
		http.Error(w, "missing test account", http.StatusUnauthorized)
		return
	}
	switch request.Method {
	case http.MethodPost:
		var body []byte
		if request.ContentLength > protocol.MaxEncodedChanges*2 {
			http.Error(w, protocol.ErrorTooLarge, http.StatusRequestEntityTooLarge)
			return
		}
		var err error
		body, err = io.ReadAll(http.MaxBytesReader(w, request.Body, protocol.MaxEncodedChanges*2))
		if err != nil {
			http.Error(w, protocol.ErrorInvalidMessage, http.StatusBadRequest)
			return
		}
		update, err := protocol.DecodeSubmit(body)
		if err != nil {
			writeProtocolError(w, err)
			return
		}
		receipt, err := room.Submit(account, update)
		if err != nil {
			writeProtocolError(w, err)
			return
		}
		writeJSON(w, http.StatusAccepted, receipt)
	case http.MethodGet:
		var after uint64
		if _, err := fmt.Sscanf(request.URL.Query().Get("after"), "%d", &after); err != nil && request.URL.Query().Get("after") != "" {
			http.Error(w, protocol.ErrorInvalidMessage, http.StatusBadRequest)
			return
		}
		writeJSON(w, http.StatusOK, room.EventsAfter(after))
	default:
		w.Header().Set("Allow", "GET, POST")
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
	}
}

func writeProtocolError(w http.ResponseWriter, err error) {
	status := http.StatusBadRequest
	if err == rooms.ErrCounterReuse {
		status = http.StatusConflict
	}
	writeJSON(w, status, map[string]string{"code": err.Error()})
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}
