package rooms

import (
	"bytes"
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"regexp"
	"time"

	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/liveops"
	"mindarchy/backend/internal/protocol"
)

const liveFlushDelay = 500 * time.Millisecond
const liveMaxPending = 1000
const liveMaxPendingBytes = 4 << 20

var operationUUID = regexp.MustCompile(`^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$`)

type LiveEvent struct {
	Type       string                      `json:"type"`
	Protocol   int                         `json:"protocol,omitempty"`
	MapID      protocol.MapID              `json:"mapId,omitempty"`
	Role       string                      `json:"role,omitempty"`
	Epoch      string                      `json:"epoch,omitempty"`
	Revision   uint64                      `json:"revision"`
	Seq        uint64                      `json:"seq"`
	State      []byte                      `json:"state,omitempty"`
	AppliedIDs []string                    `json:"appliedIds,omitempty"`
	OpID       string                      `json:"opId,omitempty"`
	Account    protocol.AccountID          `json:"accountId,omitempty"`
	Receipts   []protocol.OperationReceipt `json:"receipts,omitempty"`
	Code       string                      `json:"code,omitempty"`
}
type liveEdit struct {
	Version int                 `json:"version"`
	ID      string              `json:"id"`
	Ops     []liveops.Operation `json:"ops"`
}
type liveSubscriber struct {
	account protocol.AccountID
	send    func(LiveEvent)
}
type LiveRoom struct {
	recent         map[string]protocol.OperationReceipt
	manager        *Manager
	lock           *roomState
	id             protocol.MapID
	head           protocol.Head
	doc            liveops.Document
	epoch          string
	revision       uint64
	pending        []protocol.OperationReceipt
	pendingBytes   int
	timer          *time.Timer
	subscribers    map[uint64]liveSubscriber
	nextSubscriber uint64
	closed         bool
}

func (m *Manager) OpenLive(ctx context.Context, id protocol.MapID) (*LiveRoom, error) {
	state := m.room(id)
	state.mu.Lock()
	defer state.mu.Unlock()
	if state.live != nil && !state.live.closed {
		return state.live, nil
	}
	head, err := m.store.LoadHead(ctx, id)
	if err != nil {
		return nil, err
	}
	if head.Deleted {
		return nil, ErrAccessDenied
	}
	var raw []byte
	recent := map[string]protocol.OperationReceipt{}
	if head.BatchID != "" {
		payload, err := m.store.GetImmutable(ctx, head.BatchID)
		if err != nil {
			return nil, err
		}
		var b couch.Batch
		if err := json.Unmarshal(payload, &b); err != nil {
			return nil, err
		}
		if b.ID != head.BatchID || b.Seq != head.Seq {
			return nil, fmt.Errorf("invalid current batch")
		}
		raw = b.Changes
		for _, receipt := range b.Operations {
			recent[operationReceiptID(id, receipt.Account, receipt.ID)] = receipt
		}
	} else {
		raw, err = m.store.GetImmutable(ctx, head.SnapshotID)
		if err != nil {
			return nil, err
		}
	}
	doc, err := liveops.Normalize(raw)
	if err != nil {
		return nil, err
	}
	head, err = m.indexCommittedReceipts(ctx, id, head)
	if err != nil {
		return nil, err
	}
	if head.LiveProtocol != 2 {
		head.LiveProtocol = 2
		head, err = m.store.CompareAndSwapHead(ctx, id, head.Rev, head)
		if err != nil {
			return nil, err
		}
	}
	nonce := make([]byte, 16)
	if _, err := rand.Read(nonce); err != nil {
		return nil, err
	}
	r := &LiveRoom{manager: m, lock: state, id: id, head: head, doc: doc, epoch: hex.EncodeToString(nonce), subscribers: map[uint64]liveSubscriber{}, recent: recent}
	state.live = r
	return r, nil
}
func (r *LiveRoom) event(kind string) LiveEvent {
	state, _ := json.Marshal(r.doc)
	return LiveEvent{Type: kind, Protocol: 2, MapID: r.id, Epoch: r.epoch, Revision: r.revision, Seq: r.head.Seq, State: state}
}

// send must be nonblocking; the HTTP adapter uses bounded per-peer queues.
func (r *LiveRoom) Subscribe(account protocol.AccountID, send func(LiveEvent)) func() {
	r.lock.mu.Lock()
	r.nextSubscriber++
	id := r.nextSubscriber
	r.subscribers[id] = liveSubscriber{account, send}
	hello := r.event("hello")
	hello.Role = r.head.ACL[account]
	for _, op := range r.pending {
		if op.Account == account {
			hello.AppliedIDs = append(hello.AppliedIDs, op.ID)
		}
	}
	send(hello)
	r.lock.mu.Unlock()
	return func() { r.lock.mu.Lock(); defer r.lock.mu.Unlock(); delete(r.subscribers, id); r.evictIdleLocked() }
}
func (r *LiveRoom) evictIdleLocked() {
	if len(r.subscribers) == 0 && len(r.pending) == 0 {
		if r.timer != nil {
			r.timer.Stop()
			r.timer = nil
		}
		r.closed = true
		if r.lock.live == r {
			r.lock.live = nil
		}
	}
}
func (r *LiveRoom) broadcast(e LiveEvent) {
	for _, sub := range r.subscribers {
		if r.head.Deleted || r.head.ACL[sub.account] == "" {
			sub.send(LiveEvent{Type: "rejected", Code: protocol.ErrorAccessRevoked})
			continue
		}
		sub.send(e)
	}
}
func liveError(code string) error { return &protocol.ProtocolError{Code: code} }
func (r *LiveRoom) Submit(ctx context.Context, account protocol.AccountID, payload []byte, hash string) error {
	return r.SubmitTo(ctx, account, payload, hash, nil)
}
func (r *LiveRoom) SubmitTo(ctx context.Context, account protocol.AccountID, payload []byte, hash string, reply func(LiveEvent)) error {
	if len(payload) == 0 || len(payload) > protocol.MaxDecodedChanges {
		return liveError(protocol.ErrorTooLarge)
	}
	digest := sha256.Sum256(payload)
	if hex.EncodeToString(digest[:]) != hash {
		return liveError(protocol.ErrorInvalidMessage)
	}
	var edit liveEdit
	decoder := json.NewDecoder(bytes.NewReader(payload))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&edit); err != nil {
		return liveError(protocol.ErrorInvalidMessage)
	}
	var extra any
	if decoder.Decode(&extra) != io.EOF {
		return liveError(protocol.ErrorInvalidMessage)
	}
	if edit.Version != 2 || !operationUUID.MatchString(edit.ID) || len(edit.Ops) == 0 || len(edit.Ops) > liveops.MaxOperations {
		return liveError(protocol.ErrorInvalidMessage)
	}
	r.lock.mu.Lock()
	defer r.lock.mu.Unlock()
	if r.closed {
		return liveError(protocol.ErrorResync)
	}
	head, err := r.manager.store.LoadHead(ctx, r.id)
	if err != nil {
		return err
	}
	if head.Deleted || head.ACL[account] == "" || head.ACL[account] == "viewer" {
		return ErrAccessDenied
	}
	if head.Seq != r.head.Seq || head.BatchID != r.head.BatchID {
		if err := r.recoverCommit(ctx, head); err != nil {
			return err
		}
	}
	r.head = head
	for _, p := range r.pending {
		if p.Account == account && p.ID == edit.ID {
			if p.Hash != hash {
				return ErrCounterReuse
			}
			if reply != nil {
				e := r.event("applied")
				e.OpID = edit.ID
				e.Account = account
				reply(e)
			}
			return nil
		}
	}
	// Recent receipts are loaded with the room, avoiding a full snapshot read
	// per edit while still covering the head/receipt-materialization crash gap.
	if receipt, ok := r.recent[operationReceiptID(r.id, account, edit.ID)]; ok {
		return r.ackDuplicate(receipt, hash, reply)
	}
	if raw, err := r.manager.store.GetImmutable(ctx, operationReceiptID(r.id, account, edit.ID)); err == nil {
		var receipt protocol.OperationReceipt
		if json.Unmarshal(raw, &receipt) != nil || receipt.ID != edit.ID || receipt.Account != account {
			return fmt.Errorf("invalid operation receipt")
		}
		return r.ackDuplicate(receipt, hash, reply)
	} else if !errors.Is(err, couch.ErrNotFound) {
		return err
	}
	if len(r.pending) >= liveMaxPending || r.pendingBytes+len(payload) > liveMaxPendingBytes {
		return liveError(protocol.ErrorQuota)
	}
	next, err := liveops.Apply(r.doc, edit.Ops)
	if err != nil {
		return liveError(protocol.ErrorInvalidMessage)
	}
	r.doc = next
	r.revision++
	r.pending = append(r.pending, protocol.OperationReceipt{ID: edit.ID, Account: account, Hash: hash})
	r.pendingBytes += len(payload)
	e := r.event("applied")
	e.OpID = edit.ID
	e.Account = account
	r.broadcast(e)
	if r.timer == nil {
		r.schedule(liveFlushDelay)
	}
	if len(r.pending) >= 100 || r.pendingBytes >= 128<<10 {
		// Flush outside the reader goroutine; applied events are already enqueued.
		if r.timer != nil {
			r.timer.Stop()
			r.timer = nil
		}
		r.schedule(0)
	}
	return nil
}
func (r *LiveRoom) ackDuplicate(receipt protocol.OperationReceipt, hash string, reply func(LiveEvent)) error {
	if receipt.Hash != hash {
		return ErrCounterReuse
	}
	if reply != nil {
		e := r.event("durable")
		e.Receipts = []protocol.OperationReceipt{receipt}
		reply(e)
	}
	return nil
}
func (r *LiveRoom) schedule(delay time.Duration) {
	r.timer = time.AfterFunc(delay, func() {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()
		_ = r.Flush(ctx)
	})
}
func (r *LiveRoom) Flush(ctx context.Context) error {
	r.lock.mu.Lock()
	defer r.lock.mu.Unlock()
	if r.timer != nil {
		r.timer.Stop()
		r.timer = nil
	}
	if r.closed || len(r.pending) == 0 {
		return nil
	}
	err := r.flushLocked(ctx)
	if err != nil {
		r.broadcast(LiveEvent{Type: "rejected", Code: "storage_unavailable", Epoch: r.epoch, Revision: r.revision})
		if !r.closed {
			r.schedule(time.Second)
		}
	} else {
		r.evictIdleLocked()
	}
	return err
}
func (r *LiveRoom) flushLocked(ctx context.Context) error {
	head, err := r.manager.store.LoadHead(ctx, r.id)
	if err != nil {
		return err
	}
	if head.Seq != r.head.Seq || head.BatchID != r.head.BatchID {
		if err := r.recoverCommit(ctx, head); err != nil {
			return err
		}
	}
	if len(r.pending) == 0 {
		return nil
	}

	// Permissions are checked when each operation is accepted. Revocation
	// blocks future submissions but must not strand previously accepted edits
	// (or other collaborators' work) in an unflushable room.
	if head.Deleted {
		return ErrAccessDenied
	}

	head, err = r.manager.indexCommittedReceipts(ctx, r.id, head)
	if err != nil {
		return err
	}
	snapshot, err := liveops.Project(r.doc)
	if err != nil {
		return err
	}
	receipts := append([]protocol.OperationReceipt(nil), r.pending...)
	for i := range receipts {
		receipts[i].Seq = head.Seq + 1
	}
	batch := couch.Batch{Parent: head.BatchID, Seq: head.Seq + 1, ReceiptSeq: head.Seq + 1, Changes: snapshot, Operations: receipts}
	data, _ := json.Marshal(batch)
	hash := sha256.Sum256(data)
	batch.ID = fmt.Sprintf("map:%s:batch:%x", r.id, hash)
	data, _ = json.Marshal(batch)
	if err := r.manager.store.PutImmutable(ctx, batch.ID, data); err != nil {
		return err
	}
	head.BatchID = batch.ID
	head.Seq = batch.Seq
	committed, err := r.manager.store.CompareAndSwapHead(ctx, r.id, head.Rev, head)
	if err != nil {
		return err
	}
	r.complete(committed, receipts)
	return nil
}
func (m *Manager) CloseLive(ctx context.Context) error {
	m.mu.Lock()
	states := make([]*roomState, 0, len(m.rooms))
	for _, s := range m.rooms {
		states = append(states, s)
	}
	m.mu.Unlock()
	var result error
	for _, s := range states {
		s.mu.Lock()
		r := s.live
		s.mu.Unlock()
		if r == nil {
			continue
		}
		if err := r.Flush(ctx); err != nil {
			result = err
		}
		s.mu.Lock()
		r.closed = true
		if r.timer != nil {
			r.timer.Stop()
			r.timer = nil
		}
		s.live = nil
		s.mu.Unlock()
	}
	return result
}

func (m *Manager) LiveVersion(ctx context.Context, id protocol.MapID) (int, error) {
	head, err := m.store.LoadHead(ctx, id)
	return head.LiveProtocol, err
}

// A successful CouchDB CAS can lose its HTTP response. Confirm the reachable
// batch and exact queued receipt prefix before acknowledging that commit.
func (r *LiveRoom) recoverCommit(ctx context.Context, head protocol.Head) error {
	if head.Seq != r.head.Seq+1 || head.BatchID == "" {
		return liveError(protocol.ErrorResync)
	}
	raw, err := r.manager.store.GetImmutable(ctx, head.BatchID)
	if err != nil {
		return err
	}
	var batch couch.Batch
	if json.Unmarshal(raw, &batch) != nil || batch.ID != head.BatchID || batch.Parent != r.head.BatchID || batch.Seq != head.Seq || len(batch.Operations) == 0 || len(batch.Operations) > len(r.pending) {
		return liveError(protocol.ErrorResync)
	}
	for i, receipt := range batch.Operations {
		p := r.pending[i]
		if receipt.ID != p.ID || receipt.Account != p.Account || receipt.Hash != p.Hash || receipt.Seq != head.Seq {
			return liveError(protocol.ErrorResync)
		}
	}
	r.complete(head, batch.Operations)
	return nil
}
func (r *LiveRoom) complete(head protocol.Head, receipts []protocol.OperationReceipt) {
	r.head = head
	r.recent = map[string]protocol.OperationReceipt{}
	for _, receipt := range receipts {
		r.recent[operationReceiptID(r.id, receipt.Account, receipt.ID)] = receipt
	}
	r.pending = r.pending[len(receipts):]
	if len(r.pending) == 0 {
		r.pendingBytes = 0
	}
	e := r.event("durable")
	e.Receipts = receipts
	r.broadcast(e)
}
