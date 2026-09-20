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
	counters map[string]string
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
	state := &roomState{counters: make(map[string]string)}
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
	if hash, ok := state.counters[key]; ok {
		if hash != update.Hash {
			return protocol.Receipt{}, ErrCounterReuse
		}
		head, err := m.store.LoadHead(ctx, update.MapID)
		if err != nil {
			return protocol.Receipt{}, err
		}
		return protocol.Receipt{DeviceID: update.DeviceID, Counter: update.Counter, Hash: hash, Seq: head.Seq}, nil
	}
	head, err := m.store.LoadHead(ctx, update.MapID)
	if err == couch.ErrNotFound {
		head, err = m.store.CompareAndSwapHead(ctx, update.MapID, "", protocol.Head{})
	}
	if err != nil {
		return protocol.Receipt{}, err
	}
	batch := couch.Batch{Parent: head.BatchID, Seq: head.Seq + 1, Changes: update.Changes}
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
	state.counters[key] = update.Hash
	return protocol.Receipt{DeviceID: update.DeviceID, Counter: update.Counter, Hash: update.Hash, Seq: batch.Seq}, nil
}
