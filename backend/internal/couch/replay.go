package couch

import (
	"context"
	"encoding/json"
	"fmt"

	"mindarchy/backend/internal/collab"
	"mindarchy/backend/internal/protocol"
)

type Batch struct {
	ID      string `json:"id"`
	Parent  string `json:"parent"`
	Seq     uint64 `json:"seq"`
	Changes []byte `json:"changes"`
}

// Replay reconstructs only the chain reachable from the authoritative head.
// Orphan immutable documents are never applied.
func Replay(ctx context.Context, store Store, mapID protocol.MapID) (protocol.Head, collab.Document, error) {
	head, err := store.LoadHead(ctx, mapID)
	if err != nil {
		return protocol.Head{}, nil, err
	}
	var doc collab.Document
	if head.SnapshotID != "" {
		snapshot, err := store.GetImmutable(ctx, head.SnapshotID)
		if err != nil {
			return protocol.Head{}, nil, fmt.Errorf("load snapshot: %w", err)
		}
		doc, err = collab.Load(snapshot)
		if err != nil {
			return protocol.Head{}, nil, fmt.Errorf("decode snapshot: %w", err)
		}
	} else {
		doc = collab.New()
	}
	chain := []Batch{}
	seen := map[string]bool{}
	for id := head.BatchID; id != ""; {
		if seen[id] {
			return protocol.Head{}, nil, fmt.Errorf("batch cycle at %s", id)
		}
		seen[id] = true
		payload, err := store.GetImmutable(ctx, id)
		if err != nil {
			return protocol.Head{}, nil, fmt.Errorf("load batch %s: %w", id, err)
		}
		var batch Batch
		if err := json.Unmarshal(payload, &batch); err != nil {
			return protocol.Head{}, nil, fmt.Errorf("decode batch %s: %w", id, err)
		}
		if batch.ID != id || batch.Seq == 0 || len(batch.Changes) == 0 {
			return protocol.Head{}, nil, fmt.Errorf("invalid batch %s", id)
		}
		chain = append(chain, batch)
		id = batch.Parent
	}
	for i := len(chain) - 1; i >= 0; i-- {
		if err := doc.Apply(chain[i].Changes); err != nil {
			return protocol.Head{}, nil, fmt.Errorf("apply batch %s: %w", chain[i].ID, err)
		}
	}
	return head, doc, nil
}
