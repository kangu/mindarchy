package sharing

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"sync"
	"time"

	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/protocol"
)

type Persistence interface {
	LoadHead(context.Context, protocol.MapID) (protocol.Head, error)
	GetImmutable(context.Context, string) ([]byte, error)
	PutImmutable(context.Context, string, []byte) error
	CompareAndSwapHead(context.Context, protocol.MapID, string, protocol.Head) (protocol.Head, error)
}

var ErrNotFound = couch.ErrNotFound
var ErrForbidden = errors.New("forbidden")

type Map struct {
	ID       protocol.MapID
	Owner    protocol.AccountID
	ACL      map[protocol.AccountID]string
	Snapshot []byte
	Name     string `json:"Name"`
	head     protocol.Head
}

type invitation struct {
	MapID   protocol.MapID
	Account protocol.AccountID
	Role    string
	Expires time.Time
}

type Service struct {
	mu              sync.RWMutex
	maps            map[protocol.MapID]*Map
	invites         map[string]invitation
	persistence     Persistence
	accountResolver func(string) protocol.AccountID
}

func NewService() *Service {
	return &Service{maps: map[protocol.MapID]*Map{}, invites: map[string]invitation{}}
}

func NewPersistentService(store Persistence) *Service {
	service := NewService()
	service.persistence = store
	return service
}

func (s *Service) Create(owner protocol.AccountID, snapshot []byte, name string) Map {
	id := protocol.MapID(randomID())
	entry := &Map{ID: id, Owner: owner, ACL: map[protocol.AccountID]string{owner: "owner"}, Snapshot: append([]byte(nil), snapshot...), Name: name}
	s.mu.Lock()
	s.maps[id] = entry
	s.mu.Unlock()
	return clone(*entry)
}

func (s *Service) PersistMap(entry Map) error {
	store, ok := s.persistence.(Persistence)
	if !ok {
		return nil
	}
	digest := sha256.Sum256(entry.Snapshot)
	snapshotID := "map:" + string(entry.ID) + ":snapshot:" + hex.EncodeToString(digest[:])
	if err := store.PutImmutable(context.Background(), snapshotID, entry.Snapshot); err != nil {
		return err
	}
	acl := map[protocol.AccountID]string{}
	for account, role := range entry.ACL {
		acl[account] = role
	}
	_, err := store.CompareAndSwapHead(context.Background(), entry.ID, "", protocol.Head{SnapshotID: snapshotID, ACL: acl, Name: entry.Name, ReceiptsIndexed: true})
	return err
}

func (s *Service) Get(account protocol.AccountID, id protocol.MapID) (Map, error) {
	if _, err := s.Role(account, id); err != nil {
		return Map{}, err
	}
	if err := s.hydrateCurrent(id); s.persistence != nil && err != nil {
		return Map{}, err
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	entry, ok := s.maps[id]
	if !ok {
		return Map{}, ErrNotFound
	}
	if _, ok := entry.ACL[account]; !ok {
		return Map{}, ErrNotFound
	}
	return clone(*entry), nil
}

func (s *Service) hydrateCurrent(id protocol.MapID) error {
	store := s.persistence
	if store == nil {
		return ErrNotFound
	}
	head, err := store.LoadHead(context.Background(), id)
	if err != nil {
		return err
	}
	s.mu.RLock()
	entry, exists := s.maps[id]
	var known protocol.Head
	if exists {
		known = entry.head
	}
	s.mu.RUnlock()
	if exists && known.Rev == head.Rev && known.Seq == head.Seq && known.BatchID == head.BatchID && known.SnapshotID == head.SnapshotID {
		return nil
	}
	// The Qt protocol stores complete JSON documents in each batch. The
	// authoritative head already identifies the latest committed state.
	var effective []byte
	if head.BatchID != "" {
		payload, err := store.GetImmutable(context.Background(), head.BatchID)
		if err != nil {
			return fmt.Errorf("load current batch: %w", err)
		}
		var batch couch.Batch
		if err := json.Unmarshal(payload, &batch); err != nil {
			return err
		}
		if batch.ID != head.BatchID || batch.Seq != head.Seq || !json.Valid(batch.Changes) {
			return fmt.Errorf("invalid current batch")
		}
		effective = batch.Changes
	} else {
		effective, err = store.GetImmutable(context.Background(), head.SnapshotID)
		if err != nil {
			return fmt.Errorf("load current snapshot: %w", err)
		}
	}
	owner := protocol.AccountID("")
	for account, role := range head.ACL {
		if role == "owner" {
			owner = account
			break
		}
	}
	if owner == "" {
		return ErrNotFound
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, exists := s.maps[id]; !exists {
		s.maps[id] = &Map{ID: id, Owner: owner, ACL: head.ACL, Snapshot: effective, Name: nonEmpty(head.Name, string(id)), head: head}
		return nil
	}
	entry = s.maps[id]
	entry.Owner = owner
	entry.ACL = head.ACL
	entry.Snapshot = effective
	entry.head = head
	return nil
}

func (s *Service) Role(account protocol.AccountID, id protocol.MapID) (string, error) {
	if s.persistence != nil {
		head, err := s.persistence.LoadHead(context.Background(), id)
		if err != nil {
			return "", err
		}
		if head.Deleted || head.ACL[account] == "" {
			return "", ErrNotFound
		}
		return head.ACL[account], nil
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	entry, ok := s.maps[id]
	if !ok || entry.ACL[account] == "" {
		return "", ErrNotFound
	}
	return entry.ACL[account], nil
}

func (s *Service) SetAccountIDResolver(resolver func(string) protocol.AccountID) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.accountResolver = resolver
}

func (s *Service) resolveAccountID(account protocol.AccountID) protocol.AccountID {
	s.mu.RLock()
	resolver := s.accountResolver
	s.mu.RUnlock()
	if resolver != nil && !strings.HasPrefix(string(account), "acct_") {
		return resolver(string(account))
	}
	return account
}

func (s *Service) Invite(owner protocol.AccountID, id protocol.MapID, account protocol.AccountID, role string) (string, error) {
	if role != "editor" && role != "viewer" {
		return "", ErrForbidden
	}
	account = s.resolveAccountID(account)
	s.mu.Lock()
	defer s.mu.Unlock()
	entry, ok := s.maps[id]
	if !ok {
		return "", ErrNotFound
	}
	if entry.ACL[owner] != "owner" {
		return "", ErrForbidden
	}
	token := randomID() + randomID()
	digest := sha256.Sum256([]byte(token))
	s.invites[hex.EncodeToString(digest[:])] = invitation{MapID: id, Account: account, Role: role, Expires: time.Now().Add(7 * 24 * time.Hour)}
	if store, ok := s.persistence.(Persistence); ok {
		invite := s.invites[hex.EncodeToString(digest[:])]
		payload, _ := json.Marshal(invite)
		if err := store.PutImmutable(context.Background(), "invite:"+hex.EncodeToString(digest[:]), payload); err != nil {
			return "", err
		}
	}
	return token, nil
}

func (s *Service) Accept(account protocol.AccountID, token string) (Map, error) {
	digest := sha256.Sum256([]byte(token))
	key := hex.EncodeToString(digest[:])
	s.mu.Lock()
	invite, ok := s.invites[key]
	if !ok {
		if store, available := s.persistence.(Persistence); available {
			if payload, err := store.GetImmutable(context.Background(), "invite:"+key); err == nil {
				_ = json.Unmarshal(payload, &invite)
				ok = invite.MapID != ""
			}
		}
	}
	s.mu.Unlock()
	if !ok || time.Now().After(invite.Expires) || invite.Account != account {
		return Map{}, ErrNotFound
	}
	if err := s.hydrateCurrent(invite.MapID); err != nil && !errors.Is(err, ErrNotFound) {
		return Map{}, err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	entry, ok := s.maps[invite.MapID]
	if !ok {
		return Map{}, ErrNotFound
	}
	if existing, exists := entry.ACL[account]; exists {
		if existing == invite.Role {
			return clone(*entry), nil
		}
	}
	entry.ACL[account] = invite.Role
	if store, available := s.persistence.(Persistence); available {
		head, err := store.LoadHead(context.Background(), invite.MapID)
		if err != nil {
			return Map{}, err
		}
		acl := map[protocol.AccountID]string{}
		for candidate, candidateRole := range entry.ACL {
			acl[candidate] = candidateRole
		}
		head.ACL = acl
		if _, err := store.CompareAndSwapHead(context.Background(), invite.MapID, head.Rev, head); err != nil {
			return Map{}, err
		}
	}
	delete(s.invites, key)
	return clone(*entry), nil
}

func clone(entry Map) Map {
	acl := entry.ACL
	entry.ACL = map[protocol.AccountID]string{}
	for account, role := range acl {
		entry.ACL[account] = role
	}
	entry.Snapshot = append([]byte(nil), entry.Snapshot...)
	return entry
}
func randomID() string {
	bytes := make([]byte, 16)
	if _, err := rand.Read(bytes); err != nil {
		panic(err)
	}
	bytes[6] = bytes[6]&0x0f | 0x40
	bytes[8] = bytes[8]&0x3f | 0x80
	return fmt.Sprintf("%x-%x-%x-%x-%x", bytes[0:4], bytes[4:6], bytes[6:8], bytes[8:10], bytes[10:16])
}

func nonEmpty(preferred, fallback string) string {
	if preferred != "" {
		return preferred
	}
	return fallback
}
