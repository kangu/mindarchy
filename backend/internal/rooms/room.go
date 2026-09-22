package rooms

import (
	"errors"
	"strconv"
	"sync"

	"mindarchy/backend/internal/protocol"
)

var (
	ErrCounterReuse = errors.New(protocol.ErrorCounterReuse)
	ErrMapMismatch  = errors.New(protocol.ErrorInvalidMessage)
)

type Event struct {
	Seq      uint64
	Account  protocol.AccountID
	DeviceID protocol.DeviceID
	Counter  uint64
	Hash     string
	Changes  []byte
}

type Room struct {
	mu       sync.Mutex
	mapID    protocol.MapID
	seq      uint64
	events   []Event
	receipts map[string]protocol.Receipt
	byCount  map[string]string
}

func New(mapID protocol.MapID) *Room {
	return &Room{mapID: mapID, receipts: make(map[string]protocol.Receipt), byCount: make(map[string]string)}
}

func (r *Room) Submit(account protocol.AccountID, update protocol.Submit) (protocol.Receipt, error) {
	if update.MapID != r.mapID {
		return protocol.Receipt{}, ErrMapMismatch
	}
	key := string(account) + ":" + string(update.DeviceID) + ":" + strconv.FormatUint(update.Counter, 10)
	r.mu.Lock()
	defer r.mu.Unlock()
	if previous, ok := r.byCount[key]; ok {
		if previous != update.Hash {
			return protocol.Receipt{}, ErrCounterReuse
		}
		return r.receipts[key], nil
	}
	r.seq++
	receipt := protocol.Receipt{DeviceID: update.DeviceID, Counter: update.Counter, Hash: update.Hash, Seq: r.seq}
	r.byCount[key] = update.Hash
	r.receipts[key] = receipt
	r.events = append(r.events, Event{Seq: r.seq, Account: account, DeviceID: update.DeviceID, Counter: update.Counter, Hash: update.Hash, Changes: append([]byte(nil), update.Changes...)})
	return receipt, nil
}

func (r *Room) EventsAfter(seq uint64) []Event {
	r.mu.Lock()
	defer r.mu.Unlock()
	result := make([]Event, 0, len(r.events))
	for _, event := range r.events {
		if event.Seq > seq {
			event.Changes = append([]byte(nil), event.Changes...)
			result = append(result, event)
		}
	}
	return result
}
