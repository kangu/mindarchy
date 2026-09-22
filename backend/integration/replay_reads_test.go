package integration

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"sync"
	"testing"

	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
)

type memCASStore struct {
	mu    sync.Mutex
	docs  map[string][]byte
	heads map[protocol.MapID]protocol.Head
	revs  uint64
}

func newMemCASStore() *memCASStore {
	return &memCASStore{docs: map[string][]byte{}, heads: map[protocol.MapID]protocol.Head{}}
}

func (s *memCASStore) ListMapIDs(context.Context) ([]protocol.MapID, error) { return nil, nil }

func (s *memCASStore) LoadHead(_ context.Context, id protocol.MapID) (protocol.Head, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	head, ok := s.heads[id]
	if !ok {
		return protocol.Head{}, sharing.ErrNotFound
	}
	return head, nil
}

func (s *memCASStore) GetImmutable(_ context.Context, id string) ([]byte, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	payload, ok := s.docs[id]
	if !ok {
		return nil, sharing.ErrNotFound
	}
	return append([]byte(nil), payload...), nil
}

func (s *memCASStore) PutImmutable(_ context.Context, id string, payload []byte) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.docs[id] = append([]byte(nil), payload...)
	return nil
}

func (s *memCASStore) CompareAndSwapHead(_ context.Context, id protocol.MapID, expectedRev string, next protocol.Head) (protocol.Head, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if head, ok := s.heads[id]; ok && head.Rev != expectedRev {
		return protocol.Head{}, errors.New("conflict")
	}
	s.revs++
	next.Rev = fmt.Sprintf("rev-%d", s.revs)
	s.heads[id] = next
	return next, nil
}

func (s *memCASStore) ReplayChain(ctx context.Context, mapID protocol.MapID) ([][]byte, error) {
	head, err := s.LoadHead(ctx, mapID)
	if err != nil {
		return nil, err
	}
	chain := [][]byte{}
	seen := map[string]bool{}
	for id := head.BatchID; id != ""; {
		if seen[id] {
			return nil, fmt.Errorf("batch cycle at %s", id)
		}
		seen[id] = true
		payload, err := s.GetImmutable(ctx, id)
		if err != nil {
			return nil, err
		}
		var batch couch.Batch
		if err := json.Unmarshal(payload, &batch); err != nil {
			return nil, err
		}
		chain = append(chain, batch.Changes)
		id = batch.Parent
	}
	for i, j := 0, len(chain)-1; i < j; i, j = i+1, j-1 {
		chain[i], chain[j] = chain[j], chain[i]
	}
	return chain, nil
}

func TestReplayCommittedBatchesOnShareReads(t *testing.T) {
	store := newMemCASStore()
	service := sharing.NewPersistentService(store)
	owner := protocol.AccountID("account-owner")
	created := service.Create(owner, []byte(`{"nodes":["base"]}`), "base map")
	if err := service.PersistMap(created); err != nil {
		t.Fatal(err)
	}
	changes := []byte(`{"nodes":["base","submitted"]}`)
	digest := sha256.Sum256(changes)
	manager := rooms.NewManager(store)
	manager.CheckTarget = func(context.Context, protocol.MapID, protocol.AccountID) error { return nil }
	receipt, err := manager.Submit(context.Background(), owner, protocol.Submit{Version: protocol.Version, MapID: created.ID, DeviceID: protocol.DeviceID("1dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65"), Counter: 1, Hash: hex.EncodeToString(digest[:]), Changes: changes})
	if err != nil {
		t.Fatal(err)
	}
	if receipt.Seq != 1 {
		t.Fatalf("receipt = %+v", receipt)
	}

	fresh := sharing.NewPersistentService(store)
	got, err := fresh.Get(owner, created.ID)
	if err != nil {
		t.Fatal(err)
	}
	if got.Name != "base map" {
		t.Fatalf("fresh service name = %q, want %q", got.Name, "base map")
	}
	if string(got.Snapshot) != string(changes) {
		t.Fatalf("fresh service snapshot = %s, want %s", got.Snapshot, changes)
	}
	role, err := fresh.Role(owner, created.ID)
	if err != nil {
		t.Fatal(err)
	}
	if role != "owner" {
		t.Fatalf("role = %q, want owner", role)
	}

	rehydrated, err := service.Get(owner, created.ID)
	if err != nil {
		t.Fatal(err)
	}
	if string(rehydrated.Snapshot) != string(changes) {
		t.Fatalf("in-memory service snapshot = %s, want %s", rehydrated.Snapshot, changes)
	}
}
