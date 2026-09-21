package integration

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/cookiejar"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/coder/websocket"
	"github.com/coder/websocket/wsjson"
	"mindarchy/backend/internal/auth"
	"mindarchy/backend/internal/httpapi"
	"mindarchy/backend/internal/sharing"
)

func TestAuthenticatedSharingAndWebSocketFlow(t *testing.T) {
	accounts := map[string]string{"owner-session": "owner", "editor-session": "editor"}
	var production http.Handler
	mux := http.NewServeMux()
	mux.HandleFunc("/_session", func(w http.ResponseWriter, request *http.Request) {
		if request.Method == http.MethodPost {
			var input struct{ Name, Password string }
			if json.NewDecoder(request.Body).Decode(&input) != nil || input.Password != "password" {
				http.Error(w, "unauthorized", 401)
				return
			}
			value := input.Name + "-session"
			accounts[value] = input.Name
			http.SetCookie(w, &http.Cookie{Name: "AuthSession", Value: value, Path: "/"})
			_ = json.NewEncoder(w).Encode(map[string]any{"ok": true, "name": input.Name, "roles": []string{}})
			return
		}
		cookie, err := request.Cookie("AuthSession")
		if err != nil {
			http.Error(w, "unauthorized", 401)
			return
		}
		name, ok := accounts[cookie.Value]
		if !ok {
			http.Error(w, "unauthorized", 401)
			return
		}
		_ = json.NewEncoder(w).Encode(map[string]any{"ok": true, "name": name, "roles": []string{}})
	})
	mux.HandleFunc("/", func(w http.ResponseWriter, request *http.Request) { production.ServeHTTP(w, request) })
	server := httptest.NewServer(mux)
	defer server.Close()
	prod := httpapi.NewProductionServer(nil, nil, sharing.NewService(), auth.NewCouchSession(server.URL), nil)
	t.Cleanup(prod.Close)
	production = prod.Handler()

	owner := newTestClient(t, server.URL, "owner")
	editor := newTestClient(t, server.URL, "editor")
	ownerID := me(t, owner, server.URL)
	_ = me(t, editor, server.URL)

	response := doJSON(t, owner, http.MethodPost, server.URL+"/v1/maps", strings.NewReader(`{"title":"shared"}`), 201)
	var created struct {
		ID string `json:"id"`
	}
	decode(t, response, &created)
	if created.ID == "" {
		t.Fatal("map ID missing")
	}

	inviteBody, _ := json.Marshal(map[string]string{"account": string(me(t, editor, server.URL)), "role": "editor"})
	response = doJSON(t, owner, http.MethodPost, server.URL+"/v1/maps/"+created.ID+"/invites", strings.NewReader(string(inviteBody)), 201)
	var invite struct {
		Token string `json:"token"`
	}
	decode(t, response, &invite)
	doJSON(t, editor, http.MethodPost, server.URL+"/v1/invites/"+invite.Token+"/accept", strings.NewReader(`{}`), 200).Body.Close()

	wsURL := strings.Replace(server.URL, "http://", "ws://", 1) + "/v1/maps/" + created.ID + "/live"
	conn, _, err := websocket.Dial(context.Background(), wsURL, &websocket.DialOptions{HTTPClient: editor})
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close(websocket.StatusNormalClosure, "")
	var hello map[string]any
	if err := wsjson.Read(context.Background(), conn, &hello); err != nil {
		t.Fatal(err)
	}
	if hello["type"] != "hello" || hello["role"] != "editor" {
		t.Fatalf("hello = %v", hello)
	}
	if err := wsjson.Write(context.Background(), conn, map[string]any{"type": "submit", "changes": "test"}); err != nil {
		t.Fatal(err)
	}
	var committed = readNonPresence(t, conn)
	if committed["type"] != "rejected" {
		t.Fatalf("commit = %v", committed)
	}
	_ = ownerID
}

func newTestClient(t *testing.T, base, user string) *http.Client {
	jar, err := cookiejar.New(nil)
	if err != nil {
		t.Fatal(err)
	}
	client := &http.Client{Jar: jar}
	response := doJSON(t, client, http.MethodPost, base+"/v1/auth/session", strings.NewReader(`{"username":"`+user+`","password":"password"}`), 200)
	response.Body.Close()
	return client
}

func me(t *testing.T, client *http.Client, base string) string {
	response := doJSON(t, client, http.MethodGet, base+"/v1/me", nil, 200)
	defer response.Body.Close()
	var value struct {
		AccountID string `json:"accountId"`
	}
	if err := json.NewDecoder(response.Body).Decode(&value); err != nil {
		t.Fatal(err)
	}
	return value.AccountID
}
func doJSON(t *testing.T, client *http.Client, method, target string, body interface{ Read([]byte) (int, error) }, expected int) *http.Response {
	request, err := http.NewRequest(method, target, body)
	if err != nil {
		t.Fatal(err)
	}
	request.Header.Set("Content-Type", "application/json")
	response, err := client.Do(request)
	if err != nil {
		t.Fatal(err)
	}
	if response.StatusCode != expected {
		response.Body.Close()
		t.Fatalf("%s %s status = %d, want %d", method, target, response.StatusCode, expected)
	}
	return response
}
func decode(t *testing.T, response *http.Response, value any) {
	defer response.Body.Close()
	if err := json.NewDecoder(response.Body).Decode(value); err != nil {
		t.Fatal(err)
	}
}
