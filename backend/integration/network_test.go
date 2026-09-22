package integration

import (
	"bytes"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strconv"
	"sync"
	"testing"

	"mindarchy/backend/internal/httpapi"
	"mindarchy/backend/internal/protocol"
)

const networkMap = "a6e7db7b-81a6-43e2-a1cf-25421f4f81e1"

type peer struct {
	client  *http.Client
	baseURL string
	account string
	device  string
	counter uint64
	seen    uint64
}

func (p *peer) submit(t *testing.T, change string) protocol.Receipt {
	t.Helper()
	p.counter++
	payload := []byte(change)
	hash := sha256.Sum256(payload)
	body := map[string]any{
		"version": 1, "mapId": networkMap, "deviceId": p.device, "counter": p.counter,
		"hash": hex.EncodeToString(hash[:]), "changes": base64.StdEncoding.EncodeToString(payload),
	}
	encoded, err := json.Marshal(body)
	if err != nil {
		t.Fatal(err)
	}
	request, err := http.NewRequest(http.MethodPost, p.baseURL+"/v1/test/maps/"+networkMap+"/changes", bytes.NewReader(encoded))
	if err != nil {
		t.Fatal(err)
	}
	request.Header.Set("X-Test-Account", p.account)
	response, err := p.client.Do(request)
	if err != nil {
		t.Fatal(err)
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusAccepted {
		t.Fatalf("submit status = %d", response.StatusCode)
	}
	var receipt protocol.Receipt
	if err := json.NewDecoder(response.Body).Decode(&receipt); err != nil {
		t.Fatal(err)
	}
	return receipt
}

func (p *peer) changes(t *testing.T) []string {
	t.Helper()
	request, err := http.NewRequest(http.MethodGet, p.baseURL+"/v1/test/maps/"+networkMap+"/changes?after="+formatUint(p.seen), nil)
	if err != nil {
		t.Fatal(err)
	}
	request.Header.Set("X-Test-Account", p.account)
	response, err := p.client.Do(request)
	if err != nil {
		t.Fatal(err)
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		t.Fatalf("changes status = %d", response.StatusCode)
	}
	var events []struct {
		Seq     uint64 `json:"Seq"`
		Changes []byte `json:"Changes"`
	}
	if err := json.NewDecoder(response.Body).Decode(&events); err != nil {
		t.Fatal(err)
	}
	result := make([]string, 0, len(events))
	for _, event := range events {
		p.seen = event.Seq
		result = append(result, string(event.Changes))
	}
	return result
}

func TestTwoPeersExchangeCommittedChangesOverNetwork(t *testing.T) {
	server := httptest.NewServer(httpapi.NewTestServer(protocol.MapID(networkMap)).Handler())
	defer server.Close()
	left := &peer{client: server.Client(), baseURL: server.URL, account: "account-left", device: "1dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65"}
	right := &peer{client: server.Client(), baseURL: server.URL, account: "account-right", device: "2dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65"}
	var receipts sync.WaitGroup
	receipts.Add(2)
	go func() { defer receipts.Done(); left.submit(t, `{"op":"insert","node":"left"}`) }()
	go func() { defer receipts.Done(); right.submit(t, `{"op":"insert","node":"right"}`) }()
	receipts.Wait()
	if got := left.changes(t); len(got) != 2 {
		t.Fatalf("left received %d committed changes, want 2", len(got))
	}
	if got := right.changes(t); len(got) != 2 {
		t.Fatalf("right received %d committed changes, want 2", len(got))
	}
	if got := left.changes(t); len(got) != 0 {
		t.Fatalf("left duplicate poll received %d changes", len(got))
	}
	if got := right.changes(t); len(got) != 0 {
		t.Fatalf("right duplicate poll received %d changes", len(got))
	}
}

func formatUint(value uint64) string {
	return strconv.FormatUint(value, 10)
}
