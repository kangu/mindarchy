package integration

import (
	"context"
	"fmt"
	"net/http"
	"strings"
	"testing"

	"github.com/coder/websocket"
	"github.com/coder/websocket/wsjson"
)

func TestInviteByDisplayNameGrantsRoleAndSubmit(t *testing.T) {
	server, _, _, _ := newLiveHarness(t)
	owner := loginAs(t, server.URL, "owner")
	invitee := loginAs(t, server.URL, "invitee")
	mapID := createMapAs(t, owner, server.URL)

	response := doJSON(t, owner, http.MethodPost, server.URL+"/v1/maps/"+mapID+"/invites", strings.NewReader(`{"account":"invitee","role":"editor"}`), http.StatusCreated)
	var invite struct {
		Token string `json:"token"`
	}
	decode(t, response, &invite)
	if invite.Token == "" {
		t.Fatal("invite token missing")
	}

	response = doJSON(t, invitee, http.MethodPost, server.URL+"/v1/invites/"+invite.Token+"/accept", nil, http.StatusOK)
	var accepted struct {
		ID   string `json:"id"`
		Role string `json:"role"`
	}
	decode(t, response, &accepted)
	if accepted.ID != mapID || accepted.Role != "editor" {
		t.Fatalf("accept = id %q role %q, want id %q role editor", accepted.ID, accepted.Role, mapID)
	}

	conn := dialLive(t, invitee, server.URL, mapID)
	defer conn.Close(websocket.StatusNormalClosure, "")
	if err := wsjson.Write(context.Background(), conn, submitFrame(t, mapID, 1, []byte(`{"nodes":["invitee edit"]}`))); err != nil {
		t.Fatal(err)
	}
	committed := readNonPresence(t, conn)
	if committed["type"] != "committed" {
		t.Fatalf("invitee submit result = %v, want committed", committed)
	}
}

func TestInviteByHashStillWorksWithResolver(t *testing.T) {
	server, _, _, _ := newLiveHarness(t)
	owner := loginAs(t, server.URL, "owner")
	invitee := loginAs(t, server.URL, "invitee")
	inviteeID := accountID(t, invitee, server.URL)
	mapID := createMapAs(t, owner, server.URL)
	response := doJSON(t, owner, http.MethodPost, server.URL+"/v1/maps/"+mapID+"/invites", strings.NewReader(fmt.Sprintf(`{"account":%q,"role":"viewer"}`, inviteeID)), http.StatusCreated)
	var invite struct {
		Token string `json:"token"`
	}
	decode(t, response, &invite)
	response = doJSON(t, invitee, http.MethodPost, server.URL+"/v1/invites/"+invite.Token+"/accept", nil, http.StatusOK)
	var accepted struct {
		Role string `json:"role"`
	}
	decode(t, response, &accepted)
	if accepted.Role != "viewer" {
		t.Fatalf("accept role = %q, want viewer", accepted.Role)
	}
}
