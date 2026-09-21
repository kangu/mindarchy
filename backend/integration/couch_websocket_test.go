package integration

import (
	"context"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/cookiejar"
	"net/http/httptest"
	"net/url"
	"os"
	"strings"
	"testing"

	"github.com/coder/websocket"
	"github.com/coder/websocket/wsjson"
	"mindarchy/backend/internal/auth"
	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/httpapi"
	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
)

func TestRealCouchDBRestartAndDurableWebSocketSubmit(t *testing.T) {
	base, user, password := os.Getenv("MINDARCHY_TEST_COUCHDB_URL"), os.Getenv("MINDARCHY_TEST_COUCHDB_USER"), os.Getenv("MINDARCHY_TEST_COUCHDB_PASSWORD")
	if base == "" || user == "" || password == "" {
		t.Skip("set MINDARCHY_TEST_COUCHDB_URL, MINDARCHY_TEST_COUCHDB_USER, and MINDARCHY_TEST_COUCHDB_PASSWORD")
	}
	database := "mindarchy_test_" + strings.ToLower(randomTestID())
	createCouchDatabase(t, base, database, user, password)
	defer deleteCouchDatabase(t, base, database, user, password)
	store := couch.NewStore(base, database, user, password)

	var production http.Handler
	mux := http.NewServeMux()
	mux.HandleFunc("/_session", fakeOwnerSession)
	mux.HandleFunc("/", func(w http.ResponseWriter, request *http.Request) { production.ServeHTTP(w, request) })
	server := httptest.NewServer(mux)
	defer server.Close()
	service := sharing.NewPersistentService(store)
	production = httpapi.NewProductionServer(nil, nil, service, auth.NewCouchSession(server.URL), rooms.NewManager(store)).Handler()
	client := couchTestClient(t, server.URL)
	response := doJSON(t, client, http.MethodPost, server.URL+"/v1/maps", strings.NewReader(`{"portable":"snapshot"}`), http.StatusCreated)
	var created struct {
		ID string `json:"id"`
	}
	decode(t, response, &created)
	if created.ID == "" {
		t.Fatal("map ID missing")
	}

	// Replace the application service to simulate a process restart. The ACL and
	// snapshot must be reconstructed from the CouchDB head, not process memory.
	production = httpapi.NewProductionServer(nil, nil, sharing.NewPersistentService(store), auth.NewCouchSession(server.URL), rooms.NewManager(store)).Handler()
	response = doJSON(t, client, http.MethodGet, server.URL+"/v1/maps/"+created.ID, nil, http.StatusOK)
	response.Body.Close()

	wsURL := strings.Replace(server.URL, "http://", "ws://", 1) + "/v1/maps/" + created.ID + "/live"
	conn, _, err := websocket.Dial(context.Background(), wsURL, &websocket.DialOptions{HTTPClient: client})
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close(websocket.StatusNormalClosure, "")
	var hello map[string]any
	if err := wsjson.Read(context.Background(), conn, &hello); err != nil {
		t.Fatal(err)
	}
	changes := []byte{0}
	digest := sha256.Sum256(changes)
	mapID := protocol.MapID(created.ID)
	frame := map[string]any{"version": 1, "mapId": mapID, "deviceId": protocol.DeviceID("1dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65"), "counter": 1, "hash": fmt.Sprintf("%x", digest[:]), "changes": base64.StdEncoding.EncodeToString(changes)}
	if err := wsjson.Write(context.Background(), conn, map[string]any{"type": "submit", "changes": frame}); err != nil {
		t.Fatal(err)
	}
	var rejected map[string]any
	for {
		if err := wsjson.Read(context.Background(), conn, &rejected); err != nil {
			t.Fatal(err)
		}
		if rejected["type"] != "presence" {
			break
		}
	}
	if rejected["type"] != "rejected" || rejected["code"] != "invalid_message" {
		t.Fatalf("websocket response = %v", rejected)
	}
	head, err := store.LoadHead(context.Background(), protocol.MapID(created.ID))
	if err != nil {
		t.Fatal(err)
	}
	if head.Seq != 0 || head.BatchID != "" {
		t.Fatalf("head advanced past invalid snapshot: %+v", head)
	}

	changes = []byte(`{"portable":"restart"}`)
	digest = sha256.Sum256(changes)
	frame = map[string]any{"version": 1, "mapId": mapID, "deviceId": protocol.DeviceID("1dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65"), "counter": 1, "hash": fmt.Sprintf("%x", digest[:]), "changes": base64.StdEncoding.EncodeToString(changes)}
	if err := wsjson.Write(context.Background(), conn, map[string]any{"type": "submit", "changes": frame}); err != nil {
		t.Fatal(err)
	}
	var committed map[string]any
	for {
		if err := wsjson.Read(context.Background(), conn, &committed); err != nil {
			t.Fatal(err)
		}
		if committed["type"] != "presence" {
			break
		}
	}
	if committed["type"] != "committed" {
		t.Fatalf("websocket response = %v", committed)
	}
	head, err = store.LoadHead(context.Background(), protocol.MapID(created.ID))
	if err != nil {
		t.Fatal(err)
	}
	if head.Seq != 1 || head.BatchID == "" {
		t.Fatalf("head = %+v", head)
	}
	if _, err := store.GetImmutable(context.Background(), head.BatchID); err != nil {
		t.Fatalf("durable batch missing: %v", err)
	}
}

func fakeOwnerSession(w http.ResponseWriter, request *http.Request) {
	if request.Method == http.MethodPost {
		http.SetCookie(w, &http.Cookie{Name: "AuthSession", Value: "owner-session", Path: "/"})
		_ = json.NewEncoder(w).Encode(map[string]any{"ok": true, "name": "owner", "roles": []string{}})
		return
	}
	cookie, err := request.Cookie("AuthSession")
	if err != nil || cookie.Value != "owner-session" {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}
	_ = json.NewEncoder(w).Encode(map[string]any{"ok": true, "name": "owner", "roles": []string{}})
}

func couchTestClient(t *testing.T, base string) *http.Client {
	jar, err := cookiejar.New(nil)
	if err != nil {
		t.Fatal(err)
	}
	client := &http.Client{Jar: jar}
	response := doJSON(t, client, http.MethodPost, base+"/v1/auth/session", strings.NewReader(`{"username":"owner","password":"password"}`), http.StatusOK)
	response.Body.Close()
	return client
}
func createCouchDatabase(t *testing.T, base, database, user, password string) {
	request, _ := http.NewRequest(http.MethodPut, strings.TrimRight(base, "/")+"/"+url.PathEscape(database), nil)
	request.SetBasicAuth(user, password)
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		t.Fatal(err)
	}
	response.Body.Close()
	if response.StatusCode != http.StatusCreated && response.StatusCode != http.StatusPreconditionFailed {
		t.Fatalf("create database status %d", response.StatusCode)
	}
}
func deleteCouchDatabase(t *testing.T, base, database, user, password string) {
	request, _ := http.NewRequest(http.MethodDelete, strings.TrimRight(base, "/")+"/"+url.PathEscape(database), nil)
	request.SetBasicAuth(user, password)
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		t.Log(err)
		return
	}
	response.Body.Close()
}
func randomTestID() string { return fmt.Sprintf("%d", os.Getpid()) }
