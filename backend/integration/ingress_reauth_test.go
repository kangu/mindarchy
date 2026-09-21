package integration

import (
	"context"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"net/http/cookiejar"
	"net/http/httptest"
	"strings"
	"sync/atomic"
	"testing"
	"time"

	"github.com/coder/websocket"
	"github.com/coder/websocket/wsjson"
	"mindarchy/backend/internal/auth"
	"mindarchy/backend/internal/httpapi"
	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
)

const testDevice = "1dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65"

type fakeCouchSessions struct {
	expired atomic.Bool
}

func (f *fakeCouchSessions) handle(w http.ResponseWriter, request *http.Request) {
	if request.Method == http.MethodPost {
		var body struct {
			Name string `json:"name"`
		}
		if json.NewDecoder(request.Body).Decode(&body) != nil || body.Name == "" {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		http.SetCookie(w, &http.Cookie{Name: "AuthSession", Value: "session-" + body.Name, Path: "/"})
		_ = json.NewEncoder(w).Encode(map[string]any{"ok": true, "name": body.Name, "roles": []string{}})
		return
	}
	if f.expired.Load() {
		http.Error(w, "session expired", http.StatusUnauthorized)
		return
	}
	cookie, err := request.Cookie("AuthSession")
	if err != nil || !strings.HasPrefix(cookie.Value, "session-") {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}
	name := strings.TrimPrefix(cookie.Value, "session-")
	_ = json.NewEncoder(w).Encode(map[string]any{"ok": true, "userCtx": map[string]any{"name": name, "roles": []string{}}, "info": map[string]any{"authenticated": name}})
}

func newLiveHarness(t *testing.T, options ...httpapi.ServerOption) (*httptest.Server, *memCASStore, *fakeCouchSessions, *httpapi.ProductionServer) {
	t.Helper()
	store := newMemCASStore()
	service := sharing.NewPersistentService(store)
	sessions := &fakeCouchSessions{}
	var production http.Handler
	outer := http.NewServeMux()
	outer.HandleFunc("/_session", sessions.handle)
	outer.HandleFunc("/", func(w http.ResponseWriter, request *http.Request) { production.ServeHTTP(w, request) })
	server := httptest.NewServer(outer)
	t.Cleanup(server.Close)
	service.SetAccountIDResolver(auth.AccountIDForName)
	prod := httpapi.NewProductionServer(nil, nil, service, auth.NewCouchSession(server.URL), rooms.NewManager(store), options...)
	t.Cleanup(prod.Close)
	production = prod.Handler()
	return server, store, sessions, prod
}

func loginAs(t *testing.T, base, name string) *http.Client {
	t.Helper()
	jar, err := cookiejar.New(nil)
	if err != nil {
		t.Fatal(err)
	}
	client := &http.Client{Jar: jar}
	response := doJSON(t, client, http.MethodPost, base+"/v1/auth/session", strings.NewReader(fmt.Sprintf(`{"username":%q,"password":"password"}`, name)), http.StatusOK)
	response.Body.Close()
	return client
}

func accountID(t *testing.T, client *http.Client, base string) string {
	t.Helper()
	response := doJSON(t, client, http.MethodGet, base+"/v1/me", nil, http.StatusOK)
	var result struct {
		AccountID string `json:"accountId"`
	}
	decode(t, response, &result)
	if result.AccountID == "" {
		t.Fatal("missing account id")
	}
	return result.AccountID
}

func createMapAs(t *testing.T, client *http.Client, base string) string {
	t.Helper()
	response := doJSON(t, client, http.MethodPost, base+"/v1/maps", strings.NewReader(`{"portable":"snapshot"}`), http.StatusCreated)
	var created struct {
		ID string `json:"id"`
	}
	decode(t, response, &created)
	if created.ID == "" {
		t.Fatal("map ID missing")
	}
	return created.ID
}

func dialLive(t *testing.T, client *http.Client, base, mapID string) *websocket.Conn {
	t.Helper()
	wsURL := strings.Replace(base, "http://", "ws://", 1) + "/v1/maps/" + mapID + "/live"
	conn, _, err := websocket.Dial(context.Background(), wsURL, &websocket.DialOptions{HTTPClient: client})
	if err != nil {
		t.Fatal(err)
	}
	var hello map[string]any
	if err := wsjson.Read(context.Background(), conn, &hello); err != nil {
		t.Fatal(err)
	}
	if hello["type"] != "hello" {
		t.Fatalf("unexpected first message %v", hello)
	}
	return conn
}

func readPresenceRoster(t *testing.T, conn *websocket.Conn) []string {
	t.Helper()
	for {
		var message map[string]any
		if err := wsjson.Read(context.Background(), conn, &message); err != nil {
			t.Fatal(err)
		}
		if message["type"] != "presence" {
			t.Fatalf("expected presence roster, got %v", message)
		}
		if accounts, ok := message["accounts"].([]any); ok {
			result := make([]string, 0, len(accounts))
			for _, account := range accounts {
				result = append(result, account.(string))
			}
			return result
		}
	}
}

func readNonPresence(t *testing.T, conn *websocket.Conn) map[string]any {
	t.Helper()
	for {
		var message map[string]any
		if err := wsjson.Read(context.Background(), conn, &message); err != nil {
			t.Fatal(err)
		}
		if message["type"] != "presence" {
			return message
		}
	}
}

func submitFrame(t *testing.T, mapID string, counter uint64, changes []byte) map[string]any {
	t.Helper()
	digest := sha256.Sum256(changes)
	return map[string]any{"type": "submit", "changes": map[string]any{
		"version": protocol.Version, "mapId": mapID, "deviceId": testDevice, "counter": counter,
		"hash": fmt.Sprintf("%x", digest[:]), "changes": base64.StdEncoding.EncodeToString(changes),
	}}
}

func TestInvalidSnapshotBytesRejectedThroughProductionSocket(t *testing.T) {
	server, store, _, _ := newLiveHarness(t)
	client := loginAs(t, server.URL, "owner")
	mapID := createMapAs(t, client, server.URL)
	conn := dialLive(t, client, server.URL, mapID)
	defer conn.Close(websocket.StatusNormalClosure, "")
	if err := wsjson.Write(context.Background(), conn, submitFrame(t, mapID, 1, []byte{0})); err != nil {
		t.Fatal(err)
	}
	message := readNonPresence(t, conn)
	if message["type"] != "rejected" || message["code"] != "invalid_message" {
		t.Fatalf("expected invalid_message rejection, got %v", message)
	}
	head, err := store.LoadHead(context.Background(), protocol.MapID(mapID))
	if err != nil {
		t.Fatal(err)
	}
	if head.Seq != 0 || head.BatchID != "" {
		t.Fatalf("head advanced past invalid snapshot: %+v", head)
	}
}

func TestCrossMapSubmitRejectedViaSocket(t *testing.T) {
	server, store, _, _ := newLiveHarness(t)
	client := loginAs(t, server.URL, "owner")
	mapA := createMapAs(t, client, server.URL)
	mapB := createMapAs(t, client, server.URL)
	conn := dialLive(t, client, server.URL, mapA)
	defer conn.Close(websocket.StatusNormalClosure, "")
	if err := wsjson.Write(context.Background(), conn, submitFrame(t, mapB, 1, []byte(`{"nodes":[]}`))); err != nil {
		t.Fatal(err)
	}
	message := readNonPresence(t, conn)
	if message["type"] != "rejected" || message["code"] != "access_revoked" {
		t.Fatalf("expected access_revoked for cross-map submit, got %v", message)
	}
	head, err := store.LoadHead(context.Background(), protocol.MapID(mapA))
	if err != nil {
		t.Fatal(err)
	}
	if head.Seq != 0 || head.BatchID != "" {
		t.Fatalf("head advanced on cross-map submit: %+v", head)
	}

	denied := rooms.NewManager(store)
	denied.CheckTarget = func(context.Context, protocol.MapID, protocol.AccountID) error { return errors.New("denied") }
	if _, err := denied.Submit(context.Background(), protocol.AccountID("acct-owner"), protocol.Submit{Version: protocol.Version, MapID: protocol.MapID(mapA), DeviceID: testDevice, Counter: 1, Hash: "invalid", Changes: []byte(`{}`)}); !errors.Is(err, rooms.ErrAccessDenied) {
		t.Fatalf("CheckTarget error not surfaced as access denied: %v", err)
	}
	unconfigured := rooms.NewManager(store)
	if _, err := unconfigured.Submit(context.Background(), protocol.AccountID("acct-owner"), protocol.Submit{Version: protocol.Version, MapID: protocol.MapID(mapA), DeviceID: testDevice, Counter: 1, Hash: "invalid", Changes: []byte(`{}`)}); !errors.Is(err, rooms.ErrAccessDenied) {
		t.Fatalf("nil CheckTarget not refused: %v", err)
	}
}

func TestSubmitAfterSocketExpiryRejected(t *testing.T) {
	server, store, sessions, _ := newLiveHarness(t)
	client := loginAs(t, server.URL, "owner")
	mapID := createMapAs(t, client, server.URL)
	conn := dialLive(t, client, server.URL, mapID)
	if err := wsjson.Write(context.Background(), conn, submitFrame(t, mapID, 1, []byte(`{"nodes":["first"]}`))); err != nil {
		t.Fatal(err)
	}
	committed := readNonPresence(t, conn)
	if committed["type"] != "committed" {
		t.Fatalf("expected committed receipt, got %v", committed)
	}
	sessions.expired.Store(true)
	if err := wsjson.Write(context.Background(), conn, submitFrame(t, mapID, 2, []byte(`{"nodes":["second"]}`))); err != nil {
		t.Fatal(err)
	}
	rejected := readNonPresence(t, conn)
	if rejected["type"] != "rejected" || rejected["code"] != "access_revoked" {
		t.Fatalf("expected access_revoked after expiry, got %v", rejected)
	}
	var next map[string]any
	if err := wsjson.Read(context.Background(), conn, &next); err == nil {
		t.Fatalf("socket not closed after expiry, received %v", next)
	}
	head, err := store.LoadHead(context.Background(), protocol.MapID(mapID))
	if err != nil {
		t.Fatal(err)
	}
	if head.Seq != 1 {
		t.Fatalf("post-expiry submit persisted, head = %+v", head)
	}
}

func TestPresenceRosterBroadcastOnJoinAndExpiry(t *testing.T) {
	server, _, _, _ := newLiveHarness(t, httpapi.WithPresenceTTL(250*time.Millisecond))
	ownerClient := loginAs(t, server.URL, "owner")
	ownerAccount := accountID(t, ownerClient, server.URL)
	peerClient := loginAs(t, server.URL, "peer")
	peerAccount := accountID(t, peerClient, server.URL)
	mapID := createMapAs(t, ownerClient, server.URL)
	response := doJSON(t, ownerClient, http.MethodPost, server.URL+"/v1/maps/"+mapID+"/invites", strings.NewReader(fmt.Sprintf(`{"account":%q,"role":"editor"}`, peerAccount)), http.StatusCreated)
	var invite struct {
		Token string `json:"token"`
	}
	decode(t, response, &invite)
	response = doJSON(t, peerClient, http.MethodPost, server.URL+"/v1/invites/"+invite.Token+"/accept", nil, http.StatusOK)
	response.Body.Close()

	ownerConn := dialLive(t, ownerClient, server.URL, mapID)
	defer ownerConn.Close(websocket.StatusNormalClosure, "")
	ownerRoster := readPresenceRoster(t, ownerConn)
	if len(ownerRoster) != 1 || ownerRoster[0] != ownerAccount {
		t.Fatalf("owner join roster = %v, want [%s]", ownerRoster, ownerAccount)
	}
	peerConn := dialLive(t, peerClient, server.URL, mapID)
	defer peerConn.Close(websocket.StatusNormalClosure, "")
	peerRoster := readPresenceRoster(t, peerConn)
	want := []string{ownerAccount, peerAccount}
	if len(peerRoster) != 2 || peerRoster[0] != want[0] || peerRoster[1] != want[1] {
		t.Fatalf("peer join roster = %v, want %v", peerRoster, want)
	}
	ownerRoster = readPresenceRoster(t, ownerConn)
	if len(ownerRoster) != 2 {
		t.Fatalf("owner second roster = %v, want %v", ownerRoster, want)
	}
	time.Sleep(1500 * time.Millisecond)
	deadlineRoster := readPresenceRoster(t, ownerConn)
	for _, account := range deadlineRoster {
		if account == peerAccount {
			t.Fatalf("expired peer still in roster: %v", deadlineRoster)
		}
	}
}
