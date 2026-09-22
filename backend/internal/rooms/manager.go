package rooms

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"sync"

	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/protocol"
)

var (
	ErrAccessDenied    = errors.New("access_denied")
	ErrInvalidMessage  = errors.New(protocol.ErrorInvalidMessage)
	ErrInvalidSnapshot = errors.New("invalid_snapshot")
)

type Manager struct {
	store       couch.Store
	CheckTarget func(ctx context.Context, mapID protocol.MapID, account protocol.AccountID) error
	mu          sync.Mutex
	rooms       map[protocol.MapID]*roomState
}

type roomState struct {
	live *LiveRoom
	mu   sync.Mutex
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
	state := &roomState{}
	m.rooms[mapID] = state
	return state
}

// Submit commits an immutable batch before advancing the authoritative head.
// It serializes submissions per map and never acknowledges a speculative head.
func (m *Manager) Submit(ctx context.Context, account protocol.AccountID, update protocol.Submit) (protocol.Receipt, error) {
	receipt, _, err := m.SubmitWithStatus(ctx, account, update)
	return receipt, err
}

// SubmitWithStatus distinguishes a durable retry from a newly committed edit.
// Retries must be acknowledged only to their sender, never rebroadcast as edits.
func (m *Manager) SubmitWithStatus(ctx context.Context, account protocol.AccountID, update protocol.Submit) (protocol.Receipt, bool, error) {
	if m.CheckTarget == nil {
		return protocol.Receipt{}, false, ErrAccessDenied
	}
	if err := m.CheckTarget(ctx, update.MapID, account); err != nil {
		return protocol.Receipt{}, false, ErrAccessDenied
	}
	if len(update.Changes) > protocol.MaxDecodedChanges || !json.Valid(update.Changes) {
		return protocol.Receipt{}, false, ErrInvalidSnapshot
	}
	changesDigest := sha256.Sum256(update.Changes)
	if hex.EncodeToString(changesDigest[:]) != update.Hash {
		return protocol.Receipt{}, false, ErrInvalidMessage
	}
	state := m.room(update.MapID)
	state.mu.Lock()
	defer state.mu.Unlock()

	head, err := m.store.LoadHead(ctx, update.MapID)
	if err == couch.ErrNotFound {
		head, err = m.store.CompareAndSwapHead(ctx, update.MapID, "", protocol.Head{})
	}
	if err != nil {
		return protocol.Receipt{}, false, err
	}
	if head.LiveProtocol == 2 {
		return protocol.Receipt{}, false, &protocol.ProtocolError{Code: "upgrade_required"}
	}
	head, err = m.indexCommittedReceipts(ctx, update.MapID, head)
	if err != nil {
		return protocol.Receipt{}, false, err
	}
	if receipt, found, err := m.lookupReceipt(ctx, account, update); err != nil {
		return protocol.Receipt{}, false, err
	} else if found {
		return receipt, true, nil
	}
	batch := couch.Batch{Parent: head.BatchID, Seq: head.Seq + 1, Account: account, DeviceID: update.DeviceID, Counter: update.Counter, Hash: update.Hash, ReceiptSeq: head.Seq + 1, Changes: update.Changes}
	encoded, err := json.Marshal(batch)
	if err != nil {
		return protocol.Receipt{}, false, err
	}
	digest := sha256.Sum256(encoded)
	batch.ID = "map:" + string(update.MapID) + ":batch:" + hex.EncodeToString(digest[:])
	encoded, err = json.Marshal(batch)
	if err != nil {
		return protocol.Receipt{}, false, err
	}
	if err := m.store.PutImmutable(ctx, batch.ID, encoded); err != nil {
		return protocol.Receipt{}, false, err
	}
	head.BatchID = batch.ID
	head.Seq = batch.Seq
	if _, err := m.store.CompareAndSwapHead(ctx, update.MapID, head.Rev, head); err != nil {
		return protocol.Receipt{}, false, err
	}
	receipt := protocol.Receipt{DeviceID: update.DeviceID, Counter: update.Counter, Hash: update.Hash, Seq: batch.Seq}
	return receipt, false, nil
}
