package integration

import (
	"context"
	"github.com/coder/websocket/wsjson"
	"testing"
)

func TestOldRetryAcknowledgesSenderWithoutRollingBackPeers(t *testing.T) {
	server, _, _, _ := newLiveHarness(t)
	client := loginAs(t, server.URL, "owner")
	id := createMapAs(t, client, server.URL)
	sender := dialLive(t, client, server.URL, id)
	defer sender.CloseNow()
	peer := dialLive(t, client, server.URL, id)
	defer peer.CloseNow()
	account := accountID(t, client, server.URL)
	send := func(counter uint64, state []byte) {
		t.Helper()
		if err := wsjson.Write(context.Background(), sender, submitFrame(t, id, counter, state)); err != nil {
			t.Fatal(err)
		}
	}
	for i, state := range [][]byte{[]byte(`{"value":"A"}`), []byte(`{"value":"B"}`)} {
		send(uint64(i+1), state)
		readNonPresence(t, sender)
		assertCommitted(t, readNonPresence(t, peer), account, uint64(i+1), state)
	}
	send(1, []byte(`{"value":"A"}`))
	ack := readNonPresence(t, sender)
	if ack["type"] != "committed" {
		t.Fatalf("retry not acknowledged: %v", ack)
	}
	latest := []byte(`{"value":"C"}`)
	send(3, latest)
	readNonPresence(t, sender)
	assertCommitted(t, readNonPresence(t, peer), account, 3, latest)
}
