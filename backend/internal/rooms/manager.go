package rooms

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"sync"

	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/protocol"
)

type Manager struct {
	store couch.Store
	mu    sync.Mutex
	rooms map[protocol.MapID]*roomState
}

type roomState struct {
	mu       sync.Mutex
	counters map[string]protocol.Receipt
}

func NewManager(store couch.Store) *Manager {
	return &Manager{store: store, rooms: make(map[protocol.MapID]*roomState)}
}

func (m *Manager) room(mapID protocol.MapID) *roomState {
	m.mu.Lock()
	defer m.mu.Unlock()
	if state := m.rooms[mapID]; state != nil {
		return state
	}
	state := &roomState{counters: make(map[string]protocol.Receipt)}
	m.rooms[mapID] = state
	return state
}

// Submit commits an immutable batch before advancing the authoritative head.
// It serializes submissions per map and never acknowledges a speculative head.
func (m *Manager) Submit(ctx context.Context, account protocol.AccountID, update protocol.Submit) (protocol.Receipt, error) {
	state := m.room(update.MapID)
	state.mu.Lock()
	defer state.mu.Unlock()
	key := fmt.Sprintf("%s:%s:%d", account, update.DeviceID, update.Counter)
	if receipt, ok := state.counters[key]; ok {
		if receipt.Hash != update.Hash {
			return protocol.Receipt{}, ErrCounterReuse
		}
		return receipt, nil
	}
	head, err := m.store.LoadHead(ctx, update.MapID)
	if err == couch.ErrNotFound {
		head, err = m.store.CompareAndSwapHead(ctx, update.MapID, "", protocol.Head{})
	}
	if err != nil {
		return protocol.Receipt{}, err
	}
	if receipt, found, conflict, err := findReceipt(ctx, m.store, head.BatchID, account, update); err != nil {
		return protocol.Receipt{}, err
	} else if conflict {
		return protocol.Receipt{}, ErrCounterReuse
	} else if found {
		state.counters[key] = receipt
		return receipt, nil
	}
	batch := couch.Batch{Parent: head.BatchID, Seq: head.Seq + 1, Account: account, DeviceID: update.DeviceID, Counter: update.Counter, Hash: update.Hash, ReceiptSeq: head.Seq + 1, Changes: update.Changes}
	encoded, err := json.Marshal(batch)
	if err != nil {
		return protocol.Receipt{}, err
	}
	digest := sha256.Sum256(encoded)
	batch.ID = "map:" + string(update.MapID) + ":batch:" + hex.EncodeToString(digest[:])
	encoded, err = json.Marshal(batch)
	if err != nil {
		return protocol.Receipt{}, err
	}
	if err := m.store.PutImmutable(ctx, batch.ID, encoded); err != nil {
		return protocol.Receipt{}, err
	}
	head.BatchID = batch.ID
	head.Seq = batch.Seq
	if _, err := m.store.CompareAndSwapHead(ctx, update.MapID, head.Rev, head); err != nil {
		return protocol.Receipt{}, err
	}
	receipt := protocol.Receipt{DeviceID: update.DeviceID, Counter: update.Counter, Hash: update.Hash, Seq: batch.Seq}
	state.counters[key] = receipt
	return receipt, nil
}

func findReceipt(ctx context.Context, store couch.Store, id string, account protocol.AccountID, update protocol.Submit) (protocol.Receipt, bool, bool, error) {
	seen := map[string]bool{}
	for id != "" {
		if seen[id] {
			return protocol.Receipt{}, false, false, fmt.Errorf("batch cycle")
		}
		seen[id] = true
		data, err := store.GetImmutable(ctx, id)
		if err != nil {
			return protocol.Receipt{}, false, false, err
		}
		var batch couch.Batch
		if err := json.Unmarshal(data, &batch); err != nil {
			return protocol.Receipt{}, false, false, err
		}
		if batch.Account == account && batch.DeviceID == update.DeviceID && batch.Counter == update.Counter {
			if batch.Hash != update.Hash {
				return protocol.Receipt{}, false, true, nil
			}
			return protocol.Receipt{DeviceID: batch.DeviceID, Counter: batch.Counter, Hash: batch.Hash, Seq: batch.ReceiptSeq}, true, false, nil
		}
		id = batch.Parent
	}
	return protocol.Receipt{}, false, false, nil
}
