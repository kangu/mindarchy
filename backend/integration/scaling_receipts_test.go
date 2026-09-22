package integration

import (
	"context"
	"crypto/sha256"
	"fmt"
	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
	"strings"
	"testing"
)

func scalingSubmit(counter uint64) protocol.Submit {
	body := []byte(fmt.Sprintf(`{"value":%d}`, counter))
	hash := sha256.Sum256(body)
	return protocol.Submit{Version: 1, MapID: "map", DeviceID: "device", Counter: counter, Hash: fmt.Sprintf("%x", hash), Changes: body}
}
func scalingManager(s *countingStore) *rooms.Manager {
	m := rooms.NewManager(s)
	m.CheckTarget = func(context.Context, protocol.MapID, protocol.AccountID) error { return nil }
	return m
}
func TestDurableReceiptLookupDoesNotWalkHistory(t *testing.T) {
	ctx := context.Background()
	s := &countingStore{memCASStore: newMemCASStore()}
	// Use a persisted initial map head, as in production.
	svc := sharing.NewPersistentService(s)
	entry := svc.Create("owner", []byte(`{}`), "map")
	svc.PersistMap(entry)
	for i := uint64(1); i <= 100; i++ {
		u := scalingSubmit(i)
		u.MapID = entry.ID
		if _, err := scalingManager(s).Submit(ctx, "owner", u); err != nil {
			t.Fatal(err)
		}
	}
	s.reads = 0
	u := scalingSubmit(101)
	u.MapID = entry.ID
	if _, err := scalingManager(s).Submit(ctx, "owner", u); err != nil {
		t.Fatal(err)
	}
	if s.reads > 4 {
		t.Fatalf("new submission read %d immutable documents", s.reads)
	}
	s.reads = 0
	u = scalingSubmit(1)
	u.MapID = entry.ID
	got, err := scalingManager(s).Submit(ctx, "owner", u)
	if err != nil || got.Seq != 1 {
		t.Fatalf("retry %+v: %v", got, err)
	}
	if s.reads > 4 {
		t.Fatalf("old retry read %d documents", s.reads)
	}
	changed := scalingSubmit(999)
	changed.MapID = entry.ID
	changed.Counter = 1
	if _, err := scalingManager(s).Submit(ctx, "owner", changed); err != rooms.ErrCounterReuse {
		t.Fatalf("counter reuse: %v", err)
	}
}

type failingCommitStore struct {
	*countingStore
	failHead    bool
	failReceipt bool
}

func (s *failingCommitStore) CompareAndSwapHead(ctx context.Context, id protocol.MapID, rev string, head protocol.Head) (protocol.Head, error) {
	if s.failHead {
		return protocol.Head{}, fmt.Errorf("simulated crash before head write")
	}
	return s.countingStore.CompareAndSwapHead(ctx, id, rev, head)
}
func (s *failingCommitStore) PutImmutable(ctx context.Context, id string, body []byte) error {
	if s.failReceipt && strings.Contains(id, ":receipt:") {
		return fmt.Errorf("receipt storage unavailable")
	}
	return s.countingStore.PutImmutable(ctx, id, body)
}
func TestReceiptCrashBoundariesAndLegacyMigration(t *testing.T) {
	ctx := context.Background()
	s := &failingCommitStore{countingStore: &countingStore{memCASStore: newMemCASStore()}}
	service := sharing.NewPersistentService(s)
	entry := service.Create("owner", []byte(`{}`), "map")
	if err := service.PersistMap(entry); err != nil {
		t.Fatal(err)
	}
	newManager := func() *rooms.Manager {
		m := rooms.NewManager(s)
		m.CheckTarget = func(context.Context, protocol.MapID, protocol.AccountID) error { return nil }
		return m
	}
	u := scalingSubmit(1)
	u.MapID = entry.ID
	s.failHead = true
	if _, err := newManager().Submit(ctx, "owner", u); err == nil {
		t.Fatal("acknowledged orphan")
	}
	s.failHead = false
	// Retry the same counter with different data: the failed batch is not a receipt.
	other := scalingSubmit(2)
	other.Counter = 1
	other.MapID = entry.ID
	if got, err := newManager().Submit(ctx, "owner", other); err != nil || got.Seq != 1 {
		t.Fatalf("orphan poisoned retry: %+v %v", got, err)
	}
	// Last committed batch has not yet been indexed. A restarted manager recovers it.
	if got, err := newManager().Submit(ctx, "owner", other); err != nil || got.Seq != 1 {
		t.Fatalf("crash recovery: %+v %v", got, err)
	}
	s.failReceipt = true
	next := scalingSubmit(3)
	next.MapID = entry.ID
	if _, err := newManager().Submit(ctx, "owner", next); err == nil {
		t.Fatal("advanced while receipt repair unavailable")
	}
	head, _ := s.LoadHead(ctx, entry.ID)
	if head.Seq != 1 {
		t.Fatal("head advanced")
	}
	s.failReceipt = false
	// Simulate an old installation with committed history but no receipt index.
	head.ReceiptsIndexed = false
	if _, err := s.CompareAndSwapHead(ctx, entry.ID, head.Rev, head); err != nil {
		t.Fatal(err)
	}
	s.mu.Lock()
	for id := range s.docs {
		if strings.Contains(id, ":receipt:") {
			delete(s.docs, id)
		}
	}
	s.mu.Unlock()
	if got, err := newManager().Submit(ctx, "owner", other); err != nil || got.Seq != 1 {
		t.Fatalf("legacy receipt migration: %+v %v", got, err)
	}
	head, _ = s.LoadHead(ctx, entry.ID)
	if !head.ReceiptsIndexed {
		t.Fatal("migration marker missing")
	}
}
