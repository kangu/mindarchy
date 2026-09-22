package couch

import (
	"context"
	"encoding/json"
	"testing"

	"github.com/automerge/automerge-go"
	"mindarchy/backend/internal/protocol"
)

type replayStore struct {
	head protocol.Head
	docs map[string][]byte
}

func (s *replayStore) ListMapIDs(context.Context) ([]protocol.MapID, error) { return nil, nil }

func (s *replayStore) LoadHead(context.Context, protocol.MapID) (protocol.Head, error) {
	return s.head, nil
}
func (s *replayStore) GetImmutable(_ context.Context, id string) ([]byte, error) {
	return s.docs[id], nil
}
func (s *replayStore) PutImmutable(context.Context, string, []byte) error { return nil }
func (s *replayStore) CompareAndSwapHead(context.Context, protocol.MapID, string, protocol.Head) (protocol.Head, error) {
	return s.head, nil
}

func TestReplayUsesReachableBatchChain(t *testing.T) {
	doc := automerge.New()
	if err := doc.RootMap().Set("title", "initial"); err != nil {
		t.Fatal(err)
	}
	if _, err := doc.Commit("initial"); err != nil {
		t.Fatal(err)
	}
	snapshot := doc.Save()
	if err := doc.RootMap().Set("title", "latest"); err != nil {
		t.Fatal(err)
	}
	if _, err := doc.Commit("latest"); err != nil {
		t.Fatal(err)
	}
	changes := doc.SaveIncremental()
	batch, err := json.Marshal(Batch{ID: "batch:two", Seq: 2, Changes: changes})
	if err != nil {
		t.Fatal(err)
	}
	store := &replayStore{head: protocol.Head{SnapshotID: "snapshot:one", BatchID: "batch:two"}, docs: map[string][]byte{"snapshot:one": snapshot, "batch:two": batch}}
	_, replayed, err := Replay(context.Background(), store, protocol.MapID("map"))
	if err != nil {
		t.Fatal(err)
	}
	loaded, err := automerge.Load(mustSave(t, replayed))
	if err != nil {
		t.Fatal(err)
	}
	value, err := loaded.RootMap().Get("title")
	if err != nil {
		t.Fatal(err)
	}
	if value.Str() != "latest" {
		t.Fatalf("title = %q", value.Str())
	}
}

func mustSave(t *testing.T, doc interface{ Save() ([]byte, error) }) []byte {
	t.Helper()
	data, err := doc.Save()
	if err != nil {
		t.Fatal(err)
	}
	return data
}
