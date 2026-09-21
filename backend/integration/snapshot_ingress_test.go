package integration

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"strings"
	"testing"
)

func TestCreateMapRejectsInvalidSnapshotJSON(t *testing.T) {
	server, _, _, _ := newLiveHarness(t)
	client := loginAs(t, server.URL, "owner")
	response := doJSON(t, client, http.MethodPost, server.URL+"/v1/maps", strings.NewReader(`{"broken`), http.StatusBadRequest)
	defer response.Body.Close()
	assertInvalidMessageBody(t, response)
}

func TestCreateMapRejectsOversizedSnapshot(t *testing.T) {
	server, _, _, _ := newLiveHarness(t)
	client := loginAs(t, server.URL, "owner")
	oversized := bytes.Repeat([]byte("a"), 1<<20+1)
	oversized[0] = '{'
	response := doJSON(t, client, http.MethodPost, server.URL+"/v1/maps", bytes.NewReader(oversized), http.StatusBadRequest)
	defer response.Body.Close()
	assertInvalidMessageBody(t, response)
}

func assertInvalidMessageBody(t *testing.T, response *http.Response) {
	t.Helper()
	payload, err := io.ReadAll(response.Body)
	if err != nil {
		t.Fatal(err)
	}
	var body struct {
		Error string `json:"error"`
	}
	if err := json.Unmarshal(payload, &body); err != nil || body.Error != "invalid_message" {
		t.Fatalf("error body = %q (err %v), want {\"error\":\"invalid_message\"}", payload, err)
	}
}
