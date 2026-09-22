package integration

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"
	"time"

	"github.com/coder/websocket"
	"github.com/coder/websocket/wsjson"
	"mindarchy/backend/internal/auth"
	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/httpapi"
	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
)

func TestLiveV2WebSocketMergeDurabilityRolesAndRestart(t *testing.T) {
	server, store, _, production := newLiveHarness(t)
	exerciseV2WebSockets(t, server, store, production)
}

func TestRealCouchDBLiveV2WebSocketMergeDurabilityRolesAndRestart(t *testing.T) {
	base, user, password := os.Getenv("MINDARCHY_TEST_COUCHDB_URL"), os.Getenv("MINDARCHY_TEST_COUCHDB_USER"), os.Getenv("MINDARCHY_TEST_COUCHDB_PASSWORD")
	if base == "" || user == "" || password == "" {
		t.Skip("set MINDARCHY_TEST_COUCHDB_URL, MINDARCHY_TEST_COUCHDB_USER, and MINDARCHY_TEST_COUCHDB_PASSWORD")
	}
	database := fmt.Sprintf("mindarchy_live_v2_test_%d_%d", os.Getpid(), time.Now().UnixNano())
	createCouchDatabase(t, base, database, user, password)
	t.Cleanup(func() { deleteCouchDatabase(t, base, database, user, password) })
	store := couch.NewStore(base, database, user, password)
	if err := store.EnsureIndexes(context.Background()); err != nil {
		t.Fatal(err)
	}
	server, production := v2ServerWithStore(t, store)
	exerciseV2WebSockets(t, server, store, production)
}

func v2ServerWithStore(t *testing.T, store couch.Store) (*httptest.Server, *httpapi.ProductionServer) {
	t.Helper()
	sessions := &fakeCouchSessions{}
	var handler http.Handler
	mux := http.NewServeMux()
	mux.HandleFunc("/_session", sessions.handle)
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) { handler.ServeHTTP(w, r) })
	server := httptest.NewServer(mux)
	t.Cleanup(server.Close)
	service := sharing.NewPersistentService(store)
	service.SetAccountIDResolver(auth.AccountIDForName)
	production := httpapi.NewProductionServer(nil, nil, service, auth.NewCouchSession(server.URL), rooms.NewManager(store))
	t.Cleanup(production.Close)
	handler = production.Handler()
	return server, production
}

func dialV2(t *testing.T, client *http.Client, base, id string) (*websocket.Conn, map[string]any) {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	conn, _, err := websocket.Dial(ctx, strings.Replace(base, "http://", "ws://", 1)+"/v1/maps/"+id+"/live?protocol=2", &websocket.DialOptions{HTTPClient: client})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { conn.CloseNow() })
	hello := readV2(t, conn)
	if hello["type"] != "hello" || hello["protocol"] != float64(2) || hello["epoch"] == "" {
		t.Fatalf("v2 hello: %v", hello)
	}
	return conn, hello
}
func readV2(t *testing.T, conn *websocket.Conn) map[string]any {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	for {
		var event map[string]any
		if err := wsjson.Read(ctx, conn, &event); err != nil {
			t.Fatal(err)
		}
		if event["type"] != "presence" {
			return event
		}
	}
}
func sendV2(t *testing.T, conn *websocket.Conn, payload []byte, hash string) {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := wsjson.Write(ctx, conn, map[string]any{"type": "edit", "operation": base64.StdEncoding.EncodeToString(payload), "hash": hash}); err != nil {
		t.Fatal(err)
	}
}
func assertV2Node(t *testing.T, event map[string]any, text, notes string) {
	t.Helper()
	encoded, ok := event["state"].(string)
	if !ok {
		t.Fatalf("missing state: %v", event)
	}
	state, err := base64.StdEncoding.DecodeString(encoded)
	if err != nil {
		t.Fatal(err)
	}
	var doc struct {
		Nodes map[string]map[string]any `json:"nodes"`
	}
	if err = json.Unmarshal(state, &doc); err != nil {
		t.Fatal(err)
	}
	n := doc.Nodes["legacy:2"]
	if n["text"] != text || n["notes"] != notes {
		t.Fatalf("merged node = %v, want text=%q notes=%q", n, text, notes)
	}
}
func inviteV2Role(t *testing.T, owner, target *http.Client, base, id, name, role string) {
	t.Helper()
	response := doJSON(t, owner, http.MethodPost, base+"/v1/maps/"+id+"/invites", strings.NewReader(fmt.Sprintf(`{"account":%q,"role":%q}`, name, role)), http.StatusCreated)
	var invite struct {
		Token string `json:"token"`
	}
	decode(t, response, &invite)
	response = doJSON(t, target, http.MethodPost, base+"/v1/invites/"+invite.Token+"/accept", nil, http.StatusOK)
	response.Body.Close()
}
func assertV2Durable(t *testing.T, event map[string]any, wantSeq float64, hashes map[string]string) {
	t.Helper()
	if event["type"] != "durable" || event["seq"] != wantSeq {
		t.Fatalf("durable event: %v", event)
	}
	receipts, ok := event["receipts"].([]any)
	if !ok || len(receipts) != len(hashes) {
		t.Fatalf("grouped receipts: %v", event)
	}
	seen := map[string]bool{}
	for _, raw := range receipts {
		receipt := raw.(map[string]any)
		id := receipt["id"].(string)
		if seen[id] || hashes[id] == "" || receipt["hash"] != hashes[id] || receipt["seq"] != wantSeq || receipt["account"] == "" {
			t.Fatalf("bad durable receipt: %v", receipt)
		}
		seen[id] = true
	}
}
func exerciseV2WebSockets(t *testing.T, server *httptest.Server, store couch.Store, production *httpapi.ProductionServer) {
	t.Helper()
	owner := loginAs(t, server.URL, "owner")
	editor := loginAs(t, server.URL, "editor")
	viewer := loginAs(t, server.URL, "viewer")
	response := doJSON(t, owner, http.MethodPost, server.URL+"/v1/maps", bytes.NewReader(liveSnapshot()), http.StatusCreated)
	var created struct {
		ID string `json:"id"`
	}
	decode(t, response, &created)
	id := created.ID
	inviteV2Role(t, owner, editor, server.URL, id, "editor", "editor")
	inviteV2Role(t, owner, viewer, server.URL, id, "viewer", "viewer")
	sender, initial := dialV2(t, owner, server.URL, id)
	peer, peerInitial := dialV2(t, editor, server.URL, id)
	if initial["state"] != peerInitial["state"] || initial["revision"] != peerInitial["revision"] {
		t.Fatal("peers did not start from same base")
	}
	payloadA, hashA := editPayload(101, "text", "Editor A")
	payloadB, hashB := editPayload(102, "notes", "Editor B")
	sendV2(t, sender, payloadA, hashA)
	sendV2(t, peer, payloadB, hashB)
	for _, conn := range []*websocket.Conn{sender, peer} {
		first, second := readV2(t, conn), readV2(t, conn)
		if first["type"] != "applied" || second["type"] != "applied" || second["revision"].(float64) <= first["revision"].(float64) {
			t.Fatalf("must apply both before durable: %v, %v", first, second)
		}
		assertV2Node(t, second, "Editor A", "Editor B")
	}
	hashes := map[string]string{"00000001-0000-4000-8000-000000000101": hashA, "00000001-0000-4000-8000-000000000102": hashB}
	for _, conn := range []*websocket.Conn{sender, peer} {
		event := readV2(t, conn)
		assertV2Durable(t, event, 1, hashes)
		assertV2Node(t, event, "Editor A", "Editor B")
	}
	head, err := store.LoadHead(context.Background(), protocol.MapID(id))
	if err != nil {
		t.Fatal(err)
	}
	if head.Seq != 1 || head.LiveProtocol != 2 {
		t.Fatalf("grouped durable head: %+v", head)
	}
	readonly, hello := dialV2(t, viewer, server.URL, id)
	if hello["role"] != "viewer" {
		t.Fatalf("viewer hello: %v", hello)
	}
	sendV2(t, readonly, payloadA, hashA)
	reject := readV2(t, readonly)
	if reject["type"] != "rejected" || reject["code"] != "access_revoked" {
		t.Fatalf("viewer edited: %v", reject)
	}
	legacyURL := strings.Replace(server.URL, "http://", "ws://", 1) + "/v1/maps/" + id + "/live"
	legacy, response, err := websocket.Dial(context.Background(), legacyURL, &websocket.DialOptions{HTTPClient: owner})
	if legacy != nil {
		legacy.CloseNow()
	}
	if err == nil || response == nil || response.StatusCode != http.StatusConflict {
		t.Fatalf("v1 promotion gate: response=%v err=%v", response, err)
	}
	response.Body.Close()
	if err := wsjson.Write(context.Background(), sender, submitFrame(t, id, 1, liveSnapshot())); err != nil {
		t.Fatal(err)
	}
	reject = readV2(t, sender)
	if reject["type"] != "rejected" || reject["code"] != "upgrade_required" {
		t.Fatalf("legacy frame accepted: %v", reject)
	}
	sender.CloseNow()
	peer.CloseNow()
	readonly.CloseNow()
	production.Close()
	restarted, _ := v2ServerWithStore(t, store)
	owner = loginAs(t, restarted.URL, "owner")
	editor = loginAs(t, restarted.URL, "editor")
	sender, restartHello := dialV2(t, owner, restarted.URL, id)
	peer, _ = dialV2(t, editor, restarted.URL, id)
	if restartHello["epoch"] == initial["epoch"] {
		t.Fatal("restart reused epoch")
	}
	assertV2Node(t, restartHello, "Editor A", "Editor B")
	sendV2(t, sender, payloadA, hashA)
	retry := readV2(t, sender)
	assertV2Durable(t, retry, 1, map[string]string{"00000001-0000-4000-8000-000000000101": hashA})
	// A new operation is the barrier: any erroneous duplicate broadcast would be
	// the peer's next non-presence event rather than this new applied event.
	payloadC, hashC := editPayload(103, "notes", "After restart")
	sendV2(t, sender, payloadC, hashC)
	for _, conn := range []*websocket.Conn{sender, peer} {
		event := readV2(t, conn)
		if event["type"] != "applied" || event["opId"] != "00000001-0000-4000-8000-000000000103" {
			t.Fatalf("duplicate rebroadcast or missing new edit: %v", event)
		}
		assertV2Node(t, event, "Editor A", "After restart")
	}
	for _, conn := range []*websocket.Conn{sender, peer} {
		assertV2Durable(t, readV2(t, conn), 2, map[string]string{"00000001-0000-4000-8000-000000000103": hashC})
	}
	head, err = store.LoadHead(context.Background(), protocol.MapID(id))
	if err != nil || head.Seq != 2 {
		t.Fatalf("retry created a batch: seq=%d err=%v", head.Seq, err)
	}
}
