package integration

import (
	"context"
	"encoding/json"
	"fmt"
	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/sharing"
	"testing"
)

type countingStore struct {
	*memCASStore
	reads int
}

func (s *countingStore) GetImmutable(ctx context.Context, id string) ([]byte, error) {
	s.reads++
	return s.memCASStore.GetImmutable(ctx, id)
}
func (s *countingStore) ReplayChain(ctx context.Context, id protocol.MapID) ([][]byte, error) {
	data, err := s.memCASStore.ReplayChain(ctx, id)
	s.reads += len(data)
	return data, err
}
func TestCurrentMapReadDoesNotWalkHistory(t *testing.T) {
	for _, n := range []int{1, 1000} {
		t.Run(fmt.Sprint(n), func(t *testing.T) {
			ctx := context.Background()
			store := &countingStore{memCASStore: newMemCASStore()}
			initial := sharing.NewPersistentService(store)
			entry := initial.Create("owner", []byte(`{"nodes":[]}`), "map")
			if err := initial.PersistMap(entry); err != nil {
				t.Fatal(err)
			}
			head, _ := store.LoadHead(ctx, entry.ID)
			for i := 1; i <= n; i++ {
				id := fmt.Sprintf("batch:%d", i)
				body, _ := json.Marshal(couch.Batch{ID: id, Parent: head.BatchID, Seq: uint64(i), Changes: []byte(`{"nodes":["latest"]}`)})
				store.PutImmutable(ctx, id, body)
				head.BatchID = id
				head.Seq = uint64(i)
			}
			if _, err := store.CompareAndSwapHead(ctx, entry.ID, head.Rev, head); err != nil {
				t.Fatal(err)
			}
			fresh := sharing.NewPersistentService(store)
			store.reads = 0
			got, err := fresh.Get("owner", entry.ID)
			if err != nil {
				t.Fatal(err)
			}
			if string(got.Snapshot) != `{"nodes":["latest"]}` {
				t.Fatalf("wrong snapshot: %s", got.Snapshot)
			}
			if store.reads > 1 {
				t.Fatalf("%d immutable reads for %d updates, want at most 1", store.reads, n)
			}
			store.reads = 0
			if role, err := sharing.NewPersistentService(store).Role("owner", entry.ID); err != nil || role != "owner" {
				t.Fatalf("role %s: %v", role, err)
			}
			if store.reads != 0 {
				t.Fatalf("authorization read %d snapshots", store.reads)
			}
		})
	}
}
func TestCurrentMapReadDoesNotHideBrokenLatestBatch(t *testing.T) {
	ctx := context.Background()
	store := newMemCASStore()
	service := sharing.NewPersistentService(store)
	entry := service.Create("owner", []byte(`{"nodes":[]}`), "map")
	if err := service.PersistMap(entry); err != nil {
		t.Fatal(err)
	}
	head, _ := store.LoadHead(ctx, entry.ID)
	head.BatchID = "missing"
	head.Seq = 1
	store.CompareAndSwapHead(ctx, entry.ID, head.Rev, head)
	if _, err := service.Get("owner", entry.ID); err == nil {
		t.Fatal("returned old state despite missing committed batch")
	}
}
