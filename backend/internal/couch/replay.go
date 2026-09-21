package couch

import (
	"context"
	"encoding/json"
	"fmt"

	"mindarchy/backend/internal/collab"
	"mindarchy/backend/internal/protocol"
)

type Batch struct {
	ID         string             `json:"id"`
	Parent     string             `json:"parent"`
	Seq        uint64             `json:"seq"`
	Account    protocol.AccountID `json:"account"`
	DeviceID   protocol.DeviceID  `json:"deviceId"`
	Counter    uint64             `json:"counter"`
	Hash       string             `json:"hash"`
	ReceiptSeq uint64             `json:"receiptSeq"`
	Changes    []byte             `json:"changes"`
}

func ReplayBatched(ctx context.Context, store Store, mapID protocol.MapID) (protocol.Head, [][]byte, error) {
	head, chain, err := batchChain(ctx, store, mapID)
	if err != nil {
		return protocol.Head{}, nil, err
	}
	changes := make([][]byte, 0, len(chain))
	for _, batch := range chain {
		changes = append(changes, batch.Changes)
	}
	return head, changes, nil
}

func batchChain(ctx context.Context, store Store, mapID protocol.MapID) (protocol.Head, []Batch, error) {
	head, err := store.LoadHead(ctx, mapID)
	if err != nil {
		return protocol.Head{}, nil, err
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
	for i, j := 0, len(chain)-1; i < j; i, j = i+1, j-1 {
		chain[i], chain[j] = chain[j], chain[i]
	}
	return head, chain, nil
}

// Replay reconstructs only the chain reachable from the authoritative head.
// Orphan immutable documents are never applied.
func Replay(ctx context.Context, store Store, mapID protocol.MapID) (protocol.Head, collab.Document, error) {
	head, chain, err := batchChain(ctx, store, mapID)
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
	for _, batch := range chain {
		if err := doc.Apply(batch.Changes); err != nil {
			return protocol.Head{}, nil, fmt.Errorf("apply batch %s: %w", batch.ID, err)
		}
	}
	return head, doc, nil
}
