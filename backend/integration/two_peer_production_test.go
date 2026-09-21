package integration

import (
	"context"
	"encoding/base64"
	"fmt"
	"net/http"
	"net/http/httptest"
	"sort"
	"strings"
	"sync/atomic"
	"testing"
	"time"

	"github.com/coder/websocket"
	"github.com/coder/websocket/wsjson"
	"mindarchy/backend/internal/auth"
	"mindarchy/backend/internal/httpapi"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
)

type twoPeerHarness struct {
	store    *memCASStore
	sessions *fakeCouchSessions
	server   *httptest.Server
	handler  atomic.Value
}

func newTwoPeerHarness(t *testing.T) *twoPeerHarness {
	t.Helper()
	harness := &twoPeerHarness{store: newMemCASStore(), sessions: &fakeCouchSessions{}}
	outer := http.NewServeMux()
	outer.HandleFunc("/_session", harness.sessions.handle)
	outer.HandleFunc("/", func(w http.ResponseWriter, request *http.Request) {
		harness.handler.Load().(http.Handler).ServeHTTP(w, request)
	})
	harness.server = httptest.NewServer(outer)
	t.Cleanup(harness.server.Close)
	harness.start(t)
	return harness
}

func (h *twoPeerHarness) start(t *testing.T) {
	t.Helper()
	service := sharing.NewPersistentService(h.store)
	prod := httpapi.NewProductionServer(nil, nil, service, auth.NewCouchSession(h.server.URL), rooms.NewManager(h.store))
	t.Cleanup(prod.Close)
	h.handler.Store(prod.Handler())
}

func closeLiveSocket(t *testing.T, conn *websocket.Conn) {
	t.Helper()
	if err := conn.Close(websocket.StatusNormalClosure, ""); err != nil && websocket.CloseStatus(err) != websocket.StatusNormalClosure {
		t.Fatalf("socket close failed: %v", err)
	}
}

func assertRoster(t *testing.T, conn *websocket.Conn, want ...string) {
	t.Helper()
	roster := readPresenceRoster(t, conn)
	if len(roster) != len(want) {
		t.Fatalf("roster = %v, want %v", roster, want)
	}
	expected := append([]string(nil), want...)
	sort.Strings(expected)
	for index := range roster {
		if roster[index] != expected[index] {
			t.Fatalf("roster = %v, want %v", roster, expected)
		}
	}
}

func assertCommitted(t *testing.T, message map[string]any, wantSender string, wantSeq uint64, wantState []byte) {
	t.Helper()
	if message["type"] != "committed" {
		t.Fatalf("expected committed, got %v", message)
	}
	if message["sender"] != wantSender {
		t.Fatalf("committed sender = %v, want %s", message["sender"], wantSender)
	}
	state, err := base64.StdEncoding.DecodeString(fmt.Sprintf("%v", message["state"]))
	if err != nil {
		t.Fatalf("committed state not base64: %v", err)
	}
	if string(state) != string(wantState) {
		t.Fatalf("committed state = %s, want %s", state, wantState)
	}
	receipt, ok := message["receipt"].(map[string]any)
	if !ok {
		t.Fatalf("committed receipt missing: %v", message)
	}
	if uint64(receipt["seq"].(float64)) != wantSeq {
		t.Fatalf("committed receipt = %v, want seq %d", receipt, wantSeq)
	}
}

func assertNoCommitBeforeClose(t *testing.T, conn *websocket.Conn) {
	t.Helper()
	found := make(chan map[string]any, 4)
	go func() {
		defer close(found)
		var message map[string]any
		err := wsjson.Read(context.Background(), conn, &message)
		if err != nil {
			return
		}
		if message["type"] == "committed" {
			found <- message
		}
	}()
	select {
	case message := <-found:
		t.Fatalf("expected exactly-once commit but socket delivered %v", message)
	case <-time.After(300 * time.Millisecond):
	}
	_ = conn.CloseNow()
}

func TestTwoPeerProductionLiveEditing(t *testing.T) {
	harness := newTwoPeerHarness(t)
	server := harness.server

	ownerClient := loginAs(t, server.URL, "owner")
	editorClient := loginAs(t, server.URL, "editor")
	ownerAccount := accountID(t, ownerClient, server.URL)
	editorAccount := accountID(t, editorClient, server.URL)

	response := doJSON(t, ownerClient, http.MethodPost, server.URL+"/v1/maps", strings.NewReader(`{"name":"anchor"}`), http.StatusCreated)
	var created struct {
		ID string `json:"id"`
	}
	decode(t, response, &created)
	if created.ID == "" {
		t.Fatal("map ID missing")
	}
	response = doJSON(t, ownerClient, http.MethodPost, server.URL+"/v1/maps/"+created.ID+"/invites", strings.NewReader(fmt.Sprintf(`{"account":%q,"role":"editor"}`, editorAccount)), http.StatusCreated)
	var invite struct {
		Token string `json:"token"`
	}
	decode(t, response, &invite)
	response = doJSON(t, editorClient, http.MethodPost, server.URL+"/v1/invites/"+invite.Token+"/accept", nil, http.StatusOK)
	response.Body.Close()

	ownerConn := dialLive(t, ownerClient, server.URL, created.ID)
	defer ownerConn.Close(websocket.StatusNormalClosure, "")
	assertRoster(t, ownerConn, ownerAccount)

	editorConn := dialLive(t, editorClient, server.URL, created.ID)
	defer editorConn.Close(websocket.StatusNormalClosure, "")
	assertRoster(t, editorConn, ownerAccount, editorAccount)
	assertRoster(t, ownerConn, ownerAccount, editorAccount)

	alpha := []byte(`{"name":"alpha"}`)
	if err := wsjson.Write(context.Background(), ownerConn, submitFrame(t, created.ID, 1, alpha)); err != nil {
		t.Fatal(err)
	}
	ownerCommitted := readNonPresence(t, ownerConn)
	assertCommitted(t, ownerCommitted, ownerAccount, 1, alpha)
	editorCommitted := readNonPresence(t, editorConn)
	assertCommitted(t, editorCommitted, ownerAccount, 1, alpha)

	beta := []byte(`{"name":"beta"}`)
	if err := wsjson.Write(context.Background(), editorConn, submitFrame(t, created.ID, 1, beta)); err != nil {
		t.Fatal(err)
	}
	editorBeta := readNonPresence(t, editorConn)
	assertCommitted(t, editorBeta, editorAccount, 2, beta)
	ownerBeta := readNonPresence(t, ownerConn)
	assertCommitted(t, ownerBeta, editorAccount, 2, beta)

	assertNoCommitBeforeClose(t, ownerConn)
	assertNoCommitBeforeClose(t, editorConn)
	harness.start(t)

	ownerRejoin := dialLive(t, ownerClient, server.URL, created.ID)
	defer ownerRejoin.Close(websocket.StatusNormalClosure, "")
	assertRoster(t, ownerRejoin, ownerAccount)
	editorRejoin := dialLive(t, editorClient, server.URL, created.ID)
	defer editorRejoin.Close(websocket.StatusNormalClosure, "")
	assertRoster(t, editorRejoin, ownerAccount, editorAccount)
	assertRoster(t, ownerRejoin, ownerAccount, editorAccount)

	response = doJSON(t, editorClient, http.MethodGet, server.URL+"/v1/maps/"+created.ID, nil, http.StatusOK)
	var fetched struct {
		Snapshot []byte `json:"Snapshot"`
	}
	decode(t, response, &fetched)
	if string(fetched.Snapshot) != string(beta) {
		t.Fatalf("rehydrated snapshot = %s, want %s", fetched.Snapshot, beta)
	}
}
