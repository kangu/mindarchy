package integration

import (
	"context"
	"net"
	"net/http"
	"testing"
	"time"

	"github.com/coder/websocket"
	"github.com/coder/websocket/wsjson"
	"mindarchy/backend/internal/auth"
	"mindarchy/backend/internal/httpapi"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
)

func TestLiveWebSocketSurvivesServerTimeouts(t *testing.T) {
	store := newMemCASStore()
	service := sharing.NewPersistentService(store)
	service.SetAccountIDResolver(auth.AccountIDForName)
	sessions := &fakeCouchSessions{}
	var production http.Handler
	outer := http.NewServeMux()
	outer.HandleFunc("/_session", sessions.handle)
	outer.HandleFunc("/", func(w http.ResponseWriter, request *http.Request) { production.ServeHTTP(w, request) })
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	base := "http://" + listener.Addr().String()
	prod := httpapi.NewProductionServer(nil, nil, service, auth.NewCouchSession(base), rooms.NewManager(store))
	t.Cleanup(prod.Close)
	production = prod.Handler()
	httpServer := &http.Server{Handler: outer, ReadTimeout: 2 * time.Second, WriteTimeout: 2 * time.Second}
	go httpServer.Serve(listener)
	t.Cleanup(func() { _ = httpServer.Close() })

	client := loginAs(t, base, "owner")
	mapID := createMapAs(t, client, base)
	conn := dialLive(t, client, base, mapID)
	defer conn.Close(websocket.StatusNormalClosure, "")

	time.Sleep(3 * time.Second)
	if err := wsjson.Write(context.Background(), conn, submitFrame(t, mapID, 1, []byte(`{"nodes":["after deadline"]}`))); err != nil {
		t.Fatal(err)
	}
	committed := readNonPresence(t, conn)
	if committed["type"] != "committed" {
		t.Fatalf("submit result = %v, want committed", committed)
	}
}
