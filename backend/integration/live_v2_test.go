package integration

import (
	"context"
	"crypto/sha256"
	"encoding/json"
	"fmt"
	"sync"
	"testing"
	"time"

	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
)

func liveSnapshot() []byte {
	return []byte(`{"format":"mindarchy","version":1,"layout":"Horizontal","spacing":"Standard","branchStyle":"Rounded","themeId":"lab","manual":false,"nodes":[{"id":1,"syncId":"legacy:1","parent":-1,"children":[2],"text":"Root","notes":"","folded":false,"task":false,"checked":false,"x":0,"y":0},{"id":2,"syncId":"legacy:2","parent":1,"children":[],"text":"Child","notes":"","folded":false,"task":false,"checked":false,"x":0,"y":0}],"connections":[]}`)
}
func editPayload(n int, field, value string) ([]byte, string) {
	body, _ := json.Marshal(map[string]any{"version": 2, "id": fmt.Sprintf("00000001-0000-4000-8000-%012d", n), "ops": []any{map[string]any{"op": "set", "path": []string{"nodes", "legacy:2", field}, "value": value}}})
	hash := sha256.Sum256(body)
	return body, fmt.Sprintf("%x", hash)
}
func TestLiveV2AppliesImmediatelyAndGroupsDurableCommits(t *testing.T) {
	ctx := context.Background()
	store := newMemCASStore()
	svc := sharing.NewPersistentService(store)
	entry := svc.Create("owner", liveSnapshot(), "Live")
	if err := svc.PersistMap(entry); err != nil {
		t.Fatal(err)
	}
	m := rooms.NewManager(store)
	defer m.CloseLive(ctx)
	room, err := m.OpenLive(ctx, entry.ID)
	if err != nil {
		t.Fatal(err)
	}
	var mu sync.Mutex
	events := []rooms.LiveEvent{}
	leave := room.Subscribe("owner", func(e rooms.LiveEvent) { mu.Lock(); defer mu.Unlock(); events = append(events, e) })
	defer leave()
	for i := 1; i <= 10; i++ {
		body, hash := editPayload(i, "text", fmt.Sprint(i))
		if err := room.Submit(ctx, "owner", body, hash); err != nil {
			t.Fatal(err)
		}
	}
	head, _ := store.LoadHead(ctx, entry.ID)
	if head.Seq != 0 {
		t.Fatalf("committed before flush: %d", head.Seq)
	}
	mu.Lock()
	if len(events) != 11 || events[10].Type != "applied" {
		t.Fatalf("missing immediate updates: %v", events)
	}
	mu.Unlock()
	if err := room.Flush(ctx); err != nil {
		t.Fatal(err)
	}
	head, _ = store.LoadHead(ctx, entry.ID)
	if head.Seq != 1 {
		t.Fatalf("10 edits wrote %d batches, want 1", head.Seq)
	}
	mu.Lock()
	last := events[len(events)-1]
	mu.Unlock()
	if last.Type != "durable" || len(last.Receipts) != 10 {
		t.Fatalf("durable event %+v", last)
	}
	// Restart after the batch/head commit, before operation receipt materialization.
	restarted := rooms.NewManager(store)
	defer restarted.CloseLive(ctx)
	fresh, err := restarted.OpenLive(ctx, entry.ID)
	if err != nil {
		t.Fatal(err)
	}
	body, hash := editPayload(1, "text", "1")
	if err := fresh.Submit(ctx, "owner", body, hash); err != nil {
		t.Fatal(err)
	}
	if err := fresh.Flush(ctx); err != nil {
		t.Fatal(err)
	}
	head, _ = store.LoadHead(ctx, entry.ID)
	if head.Seq != 1 {
		t.Fatalf("retry created new commit: %d", head.Seq)
	}
}
func TestLiveV2FlushTimer(t *testing.T) {
	ctx := context.Background()
	store := newMemCASStore()
	svc := sharing.NewPersistentService(store)
	entry := svc.Create("owner", liveSnapshot(), "Live")
	svc.PersistMap(entry)
	m := rooms.NewManager(store)
	defer m.CloseLive(ctx)
	r, err := m.OpenLive(ctx, entry.ID)
	if err != nil {
		t.Fatal(err)
	}
	durable := make(chan rooms.LiveEvent, 1)
	leave := r.Subscribe("owner", func(e rooms.LiveEvent) {
		if e.Type == "durable" {
			durable <- e
		}
	})
	defer leave()
	body, hash := editPayload(1, "notes", "hello")
	if err := r.Submit(ctx, "owner", body, hash); err != nil {
		t.Fatal(err)
	}
	select {
	case <-durable:
	case <-time.After(3 * time.Second):
		t.Fatal("flush deadline missed")
	}
}
func TestLiveV2RejectsLegacyWritesAfterPromotion(t *testing.T) {
	ctx := context.Background()
	store := newMemCASStore()
	svc := sharing.NewPersistentService(store)
	entry := svc.Create("owner", liveSnapshot(), "Live")
	svc.PersistMap(entry)
	m := rooms.NewManager(store)
	defer m.CloseLive(ctx)
	m.CheckTarget = func(context.Context, protocol.MapID, protocol.AccountID) error { return nil }
	if _, err := m.OpenLive(ctx, entry.ID); err != nil {
		t.Fatal(err)
	}
	u := scalingSubmit(1)
	u.MapID = entry.ID
	if _, err := m.Submit(ctx, "owner", u); err == nil {
		t.Fatal("legacy snapshot overwrote operation map")
	}
}

type lostCommitReplyStore struct {
	*memCASStore
	lose bool
}

func (s *lostCommitReplyStore) CompareAndSwapHead(ctx context.Context, id protocol.MapID, rev string, next protocol.Head) (protocol.Head, error) {
	head, err := s.memCASStore.CompareAndSwapHead(ctx, id, rev, next)
	if err == nil && s.lose && next.Seq > 0 {
		s.lose = false
		return protocol.Head{}, fmt.Errorf("connection lost after committed head")
	}
	return head, err
}
func TestLiveV2RecoversAmbiguousCommitWithoutDuplicateWrite(t *testing.T) {
	ctx := context.Background()
	store := &lostCommitReplyStore{memCASStore: newMemCASStore()}
	svc := sharing.NewPersistentService(store)
	entry := svc.Create("owner", liveSnapshot(), "Live")
	svc.PersistMap(entry)
	m := rooms.NewManager(store)
	defer m.CloseLive(ctx)
	r, err := m.OpenLive(ctx, entry.ID)
	if err != nil {
		t.Fatal(err)
	}
	var durable int
	leave := r.Subscribe("owner", func(e rooms.LiveEvent) {
		if e.Type == "durable" {
			durable++
		}
	})
	defer leave()
	body, hash := editPayload(1, "text", "survives ambiguous reply")
	if err := r.Submit(ctx, "owner", body, hash); err != nil {
		t.Fatal(err)
	}
	store.lose = true
	if err := r.Flush(ctx); err == nil {
		t.Fatal("expected uncertain commit failure")
	}
	if durable != 0 {
		t.Fatal("acknowledged without confirming head")
	}
	if err := r.Flush(ctx); err != nil {
		t.Fatalf("cannot recover confirmed commit: %v", err)
	}
	head, _ := store.LoadHead(ctx, entry.ID)
	if durable != 1 || head.Seq != 1 {
		t.Fatalf("durable=%d seq=%d", durable, head.Seq)
	}
}

func TestLiveV2RevocationBlocksNewEditsButDrainsAlreadyAccepted(t *testing.T) {
	ctx := context.Background()
	store := newMemCASStore()
	svc := sharing.NewPersistentService(store)
	entry := svc.Create("owner", liveSnapshot(), "Live")
	svc.PersistMap(entry)
	head, _ := store.LoadHead(ctx, entry.ID)
	head.ACL["editor"] = "editor"
	store.CompareAndSwapHead(ctx, entry.ID, head.Rev, head)
	m := rooms.NewManager(store)
	defer m.CloseLive(ctx)
	r, err := m.OpenLive(ctx, entry.ID)
	if err != nil {
		t.Fatal(err)
	}
	leave := r.Subscribe("owner", func(rooms.LiveEvent) {})
	defer leave()
	body, hash := editPayload(1, "notes", "accepted before revocation")
	if err := r.Submit(ctx, "editor", body, hash); err != nil {
		t.Fatal(err)
	}
	head, _ = store.LoadHead(ctx, entry.ID)
	delete(head.ACL, "editor")
	store.CompareAndSwapHead(ctx, entry.ID, head.Rev, head)
	body, hash = editPayload(2, "notes", "unauthorized new edit")
	if err := r.Submit(ctx, "editor", body, hash); err == nil {
		t.Fatal("revoked editor submitted")
	}
	body, hash = editPayload(3, "text", "owner edit")
	if err := r.Submit(ctx, "owner", body, hash); err != nil {
		t.Fatal(err)
	}
	if err := r.Flush(ctx); err != nil {
		t.Fatalf("accepted work cannot drain: %v", err)
	}
	head, _ = store.LoadHead(ctx, entry.ID)
	if head.Seq != 1 {
		t.Fatal("failed to save accepted batch")
	}
}
