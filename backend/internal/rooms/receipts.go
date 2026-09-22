package rooms

import (
	"context"
	"crypto/sha256"
	"encoding/json"
	"errors"
	"fmt"

	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/protocol"
)

func receiptID(mapID protocol.MapID, account protocol.AccountID, device protocol.DeviceID, counter uint64) string {
	key, _ := json.Marshal([]any{account, device, counter})
	digest := sha256.Sum256(key)
	return fmt.Sprintf("map:%s:receipt:%x", mapID, digest)
}
func batchReceipt(b couch.Batch) protocol.Receipt {
	return protocol.Receipt{DeviceID: b.DeviceID, Counter: b.Counter, Hash: b.Hash, Seq: b.ReceiptSeq}
}

// Only batches reached through an authoritative head can produce receipts.
// The latest batch is itself the durable receipt until the next submission
// indexes it. Repair happens BEFORE another head can supersede that batch.
func (m *Manager) indexCommittedReceipts(ctx context.Context, id protocol.MapID, head protocol.Head) (protocol.Head, error) {
	seen := map[string]bool{}
	for batchID := head.BatchID; batchID != ""; {
		if seen[batchID] {
			return head, fmt.Errorf("batch cycle")
		}
		seen[batchID] = true
		payload, err := m.store.GetImmutable(ctx, batchID)
		if err != nil {
			return head, err
		}
		var batch couch.Batch
		if err := json.Unmarshal(payload, &batch); err != nil {
			return head, err
		}
		if batch.ID != batchID || batch.Seq == 0 || batch.ReceiptSeq != batch.Seq {
			return head, fmt.Errorf("invalid receipt batch")
		}
		if len(batch.Operations) > 0 {
			values := make(map[string][]byte, len(batch.Operations))
			for _, op := range batch.Operations {
				if op.Seq != batch.Seq || op.ID == "" || op.Account == "" {
					return head, fmt.Errorf("invalid operation receipt")
				}
				payload, _ := json.Marshal(op)
				values[operationReceiptID(id, op.Account, op.ID)] = payload
			}
			if bulk, ok := m.store.(interface {
				PutImmutables(context.Context, map[string][]byte) error
			}); ok {
				if err := bulk.PutImmutables(ctx, values); err != nil {
					return head, err
				}
			} else {
				for key, value := range values {
					if err := m.store.PutImmutable(ctx, key, value); err != nil {
						return head, err
					}
				}
			}
		} else {
			receipt, _ := json.Marshal(batchReceipt(batch))
			if err := m.store.PutImmutable(ctx, receiptID(id, batch.Account, batch.DeviceID, batch.Counter), receipt); err != nil {
				return head, err
			}
		}
		if head.ReceiptsIndexed {
			break
		}
		batchID = batch.Parent
	}
	if !head.ReceiptsIndexed {
		head.ReceiptsIndexed = true
		return m.store.CompareAndSwapHead(ctx, id, head.Rev, head)
	}
	return head, nil
}
func (m *Manager) lookupReceipt(ctx context.Context, account protocol.AccountID, u protocol.Submit) (protocol.Receipt, bool, error) {
	body, err := m.store.GetImmutable(ctx, receiptID(u.MapID, account, u.DeviceID, u.Counter))
	if errors.Is(err, couch.ErrNotFound) {
		return protocol.Receipt{}, false, nil
	}
	if err != nil {
		return protocol.Receipt{}, false, err
	}
	var receipt protocol.Receipt
	if err := json.Unmarshal(body, &receipt); err != nil {
		return receipt, false, err
	}
	if receipt.Counter != u.Counter || receipt.DeviceID != u.DeviceID || receipt.Seq == 0 {
		return receipt, false, fmt.Errorf("invalid receipt")
	}
	if receipt.Hash != u.Hash {
		return receipt, false, ErrCounterReuse
	}
	return receipt, true, nil
}

func operationReceiptID(mapID protocol.MapID, account protocol.AccountID, id string) string {
	key, _ := json.Marshal([]any{account, id})
	digest := sha256.Sum256(key)
	return fmt.Sprintf("map:%s:operation:%x", mapID, digest)
}
