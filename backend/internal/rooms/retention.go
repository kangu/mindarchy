package rooms

import (
	"context"
	"crypto/sha256"
	"encoding/json"
	"fmt"
	"strings"
	"time"

	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/protocol"
)

type garbagePlan struct {
	NotBefore time.Time `json:"notBefore"`
	IDs       []string  `json:"ids"`
}

// Checkpoint is an explicit administrative operation, never an implicit save.
// It retains at least one batch and all durable receipts for offline retries.
func (m *Manager) Checkpoint(ctx context.Context, id protocol.MapID, keep int, grace time.Duration, now time.Time) (int, error) {
	if keep < 1 || grace < time.Hour {
		return 0, fmt.Errorf("keep must be positive; grace must be at least one hour")
	}
	state := m.room(id)
	state.mu.Lock()
	defer state.mu.Unlock()
	head, err := m.store.LoadHead(ctx, id)
	if err != nil {
		return 0, err
	}
	if head.GarbageID != "" {
		return 0, fmt.Errorf("collect pending checkpoint before creating another")
	}
	head, err = m.indexCommittedReceipts(ctx, id, head)
	if err != nil {
		return 0, err
	}
	var boundary couch.Batch
	garbage := []string{}
	seen := map[string]bool{}
	count := 0
	for batchID := head.BatchID; batchID != ""; {
		if seen[batchID] {
			return 0, fmt.Errorf("batch cycle")
		}
		seen[batchID] = true
		payload, err := m.store.GetImmutable(ctx, batchID)
		if err != nil {
			return 0, err
		}
		var batch couch.Batch
		if err := json.Unmarshal(payload, &batch); err != nil {
			return 0, err
		}
		if batch.ID != batchID || batch.Seq == 0 || !json.Valid(batch.Changes) {
			return 0, fmt.Errorf("invalid checkpoint batch")
		}
		count++
		if count == keep {
			boundary = batch
		}
		if count > keep {
			garbage = append(garbage, batchID)
		}
		// An earlier checkpoint's parent may already have been deleted.
		if batch.Seq <= head.SnapshotSeq {
			break
		}
		batchID = batch.Parent
	}
	if len(garbage) == 0 {
		return 0, nil
	}
	digest := sha256.Sum256(boundary.Changes)
	snapshotID := fmt.Sprintf("map:%s:snapshot:%d:%x", id, boundary.Seq, digest)
	if err := m.store.PutImmutable(ctx, snapshotID, boundary.Changes); err != nil {
		return 0, err
	}
	if head.SnapshotID != "" && head.SnapshotID != snapshotID {
		garbage = append(garbage, head.SnapshotID)
	}
	plan := garbagePlan{NotBefore: now.Add(grace), IDs: garbage}
	body, _ := json.Marshal(plan)
	planHash := sha256.Sum256(body)
	planID := fmt.Sprintf("map:%s:gc:%x", id, planHash)
	if err := m.store.PutImmutable(ctx, planID, body); err != nil {
		return 0, err
	}
	head.SnapshotID = snapshotID
	head.SnapshotSeq = boundary.Seq
	head.GarbageID = planID
	if _, err := m.store.CompareAndSwapHead(ctx, id, head.Rev, head); err != nil {
		return 0, err
	}
	return len(garbage), nil
}

func (m *Manager) Collect(ctx context.Context, id protocol.MapID, now time.Time) (int, error) {
	deleter, ok := m.store.(interface {
		DeleteImmutable(context.Context, string) error
	})
	if !ok {
		return 0, fmt.Errorf("store does not support retention")
	}
	state := m.room(id)
	state.mu.Lock()
	defer state.mu.Unlock()
	head, err := m.store.LoadHead(ctx, id)
	if err != nil {
		return 0, err
	}
	if head.GarbageID == "" {
		return 0, nil
	}
	body, err := m.store.GetImmutable(ctx, head.GarbageID)
	if err != nil {
		return 0, err
	}
	var plan garbagePlan
	if err := json.Unmarshal(body, &plan); err != nil {
		return 0, err
	}
	if now.Before(plan.NotBefore) {
		return 0, fmt.Errorf("checkpoint grace period ends %s", plan.NotBefore.Format(time.RFC3339))
	}
	// Validate the entire plan before any deletion. Receipt records never expire.
	for _, candidate := range plan.IDs {
		if candidate == head.BatchID || candidate == head.SnapshotID || (!strings.HasPrefix(candidate, "map:"+string(id)+":batch:") && !strings.HasPrefix(candidate, "map:"+string(id)+":snapshot:")) {
			return 0, fmt.Errorf("unsafe garbage plan")
		}
	}
	for i, candidate := range plan.IDs {
		if err := deleter.DeleteImmutable(ctx, candidate); err != nil {
			return i, err
		}
	}
	head.GarbageID = ""
	if _, err := m.store.CompareAndSwapHead(ctx, id, head.Rev, head); err != nil {
		return len(plan.IDs), err
	}
	return len(plan.IDs), nil
}
