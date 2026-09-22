package integration

import (
	"context"
	"crypto/sha256"
	"fmt"
	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
	"testing"
	"time"
)

type retentionStore struct {
	*countingStore
	deleted    int
	tombstones map[string]bool
}

func (s *retentionStore) DeleteImmutable(_ context.Context, id string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.docs, id)
	if s.tombstones == nil {
		s.tombstones = map[string]bool{}
	}
	s.tombstones[id] = true
	s.deleted++
	return nil
}
func TestCheckpointGraceAndOfflineRetry(t *testing.T) {
	ctx := context.Background()
	s := &retentionStore{countingStore: &countingStore{memCASStore: newMemCASStore()}}
	svc := sharing.NewPersistentService(s)
	entry := svc.Create("owner", []byte(`{}`), "map")
	if err := svc.PersistMap(entry); err != nil {
		t.Fatal(err)
	}
	manager := rooms.NewManager(s)
	manager.CheckTarget = func(context.Context, protocol.MapID, protocol.AccountID) error { return nil }
	for i := uint64(1); i <= 10; i++ {
		u := scalingSubmit(i)
		u.MapID = entry.ID
		if _, err := manager.Submit(ctx, "owner", u); err != nil {
			t.Fatal(err)
		}
	}
	now := time.Now().UTC()
	count, err := manager.Checkpoint(ctx, entry.ID, 3, time.Hour, now)
	if err != nil {
		t.Fatal(err)
	}
	if count != 8 {
		t.Fatalf("scheduled %d deletions, want 7 old batches + initial snapshot", count)
	}
	if _, err := manager.Collect(ctx, entry.ID, now); err == nil {
		t.Fatal("collected before grace period")
	}
	if s.deleted != 0 {
		t.Fatal("deleted early")
	}
	if count, err := manager.Collect(ctx, entry.ID, now.Add(2*time.Hour)); err != nil || count != 8 {
		t.Fatalf("collect %d: %v", count, err)
	}
	latest, err := sharing.NewPersistentService(s).Get("owner", entry.ID)
	if err != nil || string(latest.Snapshot) != `{"value":10}` {
		t.Fatalf("lost current state: %s %v", latest.Snapshot, err)
	}
	retry := scalingSubmit(1)
	retry.MapID = entry.ID
	fresh := rooms.NewManager(s)
	fresh.CheckTarget = manager.CheckTarget
	if got, err := fresh.Submit(ctx, "owner", retry); err != nil || got.Seq != 1 {
		t.Fatalf("offline retry %+v: %v", got, err)
	}
	next := scalingSubmit(11)
	next.MapID = entry.ID
	if got, err := fresh.Submit(ctx, "owner", next); err != nil || got.Seq != 11 {
		t.Fatalf("next submit %+v: %v", got, err)
	}

}

func (s *retentionStore) PutImmutable(ctx context.Context, id string, body []byte) error {
	s.mu.Lock()
	deleted := s.tombstones[id]
	s.mu.Unlock()
	if deleted {
		return couch.ErrConflict
	}
	return s.countingStore.PutImmutable(ctx, id, body)
}
func TestCheckpointCanReturnToCollectedSnapshotContents(t *testing.T) {
	ctx := context.Background()
	s := &retentionStore{countingStore: &countingStore{memCASStore: newMemCASStore()}}
	svc := sharing.NewPersistentService(s)
	entry := svc.Create("owner", []byte(`{}`), "map")
	if err := svc.PersistMap(entry); err != nil {
		t.Fatal(err)
	}
	m := rooms.NewManager(s)
	m.CheckTarget = func(context.Context, protocol.MapID, protocol.AccountID) error { return nil }
	for i := uint64(1); i <= 2; i++ {
		u := scalingSubmit(i)
		u.MapID = entry.ID
		if _, err := m.Submit(ctx, "owner", u); err != nil {
			t.Fatal(err)
		}
	}
	now := time.Now().UTC()
	if _, err := m.Checkpoint(ctx, entry.ID, 1, time.Hour, now); err != nil {
		t.Fatal(err)
	}
	if _, err := m.Collect(ctx, entry.ID, now.Add(2*time.Hour)); err != nil {
		t.Fatal(err)
	}
	u := scalingSubmit(3)
	u.MapID = entry.ID
	u.Changes = []byte(`{}`)
	hash := sha256.Sum256(u.Changes)
	u.Hash = fmt.Sprintf("%x", hash)
	if _, err := m.Submit(ctx, "owner", u); err != nil {
		t.Fatal(err)
	}
	if _, err := m.Checkpoint(ctx, entry.ID, 1, time.Hour, now.Add(3*time.Hour)); err != nil {
		t.Fatalf("cannot checkpoint repeated historical contents: %v", err)
	}
}
