package sharing

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"sync"
	"time"

	"mindarchy/backend/internal/protocol"
)

type Persistence interface {
	ListMapIDs(context.Context) ([]protocol.MapID, error)
	LoadHead(context.Context, protocol.MapID) (protocol.Head, error)
	GetImmutable(context.Context, string) ([]byte, error)
	PutImmutable(context.Context, string, []byte) error
	CompareAndSwapHead(context.Context, protocol.MapID, string, protocol.Head) (protocol.Head, error)
}

var ErrNotFound = errors.New("not_found")
var ErrForbidden = errors.New("forbidden")

type Map struct {
	ID       protocol.MapID
	Owner    protocol.AccountID
	ACL      map[protocol.AccountID]string
	Snapshot []byte
}

type invitation struct {
	MapID   protocol.MapID
	Account protocol.AccountID
	Role    string
	Expires time.Time
}

type Service struct {
	mu          sync.RWMutex
	maps        map[protocol.MapID]*Map
	invites     map[string]invitation
	persistence Persistence
}

func NewService() *Service {
	return &Service{maps: map[protocol.MapID]*Map{}, invites: map[string]invitation{}}
}

func NewPersistentService(store Persistence) *Service {
	service := NewService()
	service.persistence = store
	return service
}

func (s *Service) Create(owner protocol.AccountID, snapshot []byte) Map {
	id := protocol.MapID(randomID())
	entry := &Map{ID: id, Owner: owner, ACL: map[protocol.AccountID]string{owner: "owner"}, Snapshot: append([]byte(nil), snapshot...)}
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
	_, err := store.CompareAndSwapHead(context.Background(), entry.ID, "", protocol.Head{SnapshotID: snapshotID, ACL: acl})
	return err
}

func (s *Service) List(account protocol.AccountID) []Map {
	if store, ok := s.persistence.(Persistence); ok {
		if ids, err := store.ListMapIDs(context.Background()); err == nil {
			for _, id := range ids {
				_ = s.hydrate(id)
			}
		}
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	result := []Map{}
	for _, entry := range s.maps {
		if _, ok := entry.ACL[account]; ok {
			result = append(result, clone(*entry))
		}
	}
	return result
}

func (s *Service) Get(account protocol.AccountID, id protocol.MapID) (Map, error) {
	if err := s.hydrate(id); err != nil && !errors.Is(err, ErrNotFound) {
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

func (s *Service) hydrate(id protocol.MapID) error {
	s.mu.RLock()
	_, exists := s.maps[id]
	s.mu.RUnlock()
	if exists {
		return nil
	}
	store, ok := s.persistence.(Persistence)
	if !ok {
		return ErrNotFound
	}
	head, err := store.LoadHead(context.Background(), id)
	if err != nil {
		return err
	}
	snapshot, err := store.GetImmutable(context.Background(), head.SnapshotID)
	if err != nil {
		return err
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
	if _, exists := s.maps[id]; !exists {
		s.maps[id] = &Map{ID: id, Owner: owner, ACL: head.ACL, Snapshot: snapshot}
	}
	s.mu.Unlock()
	return nil
}

func (s *Service) Role(account protocol.AccountID, id protocol.MapID) (string, error) {
	if err := s.hydrate(id); err != nil && !errors.Is(err, ErrNotFound) {
		return "", err
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	entry, ok := s.maps[id]
	if !ok {
		return "", ErrNotFound
	}
	role, ok := entry.ACL[account]
	if !ok {
		return "", ErrNotFound
	}
	return role, nil
}

func (s *Service) Invite(owner protocol.AccountID, id protocol.MapID, account protocol.AccountID, role string) (string, error) {
	if role != "editor" && role != "viewer" {
		return "", ErrForbidden
	}
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
	defer s.mu.Unlock()
	invite, ok := s.invites[key]
	if !ok {
		if store, available := s.persistence.(Persistence); available {
			if payload, err := store.GetImmutable(context.Background(), "invite:"+key); err == nil {
				_ = json.Unmarshal(payload, &invite)
				ok = invite.MapID != ""
			}
		}
	}
	if !ok || time.Now().After(invite.Expires) || invite.Account != account {
		return Map{}, ErrNotFound
	}
	entry, ok := s.maps[invite.MapID]
	if !ok {
		store, available := s.persistence.(Persistence)
		if !available {
			return Map{}, ErrNotFound
		}
		head, err := store.LoadHead(context.Background(), invite.MapID)
		if err != nil {
			return Map{}, err
		}
		snapshot, err := store.GetImmutable(context.Background(), head.SnapshotID)
		if err != nil {
			return Map{}, err
		}
		owner := protocol.AccountID("")
		for candidate, candidateRole := range head.ACL {
			if candidateRole == "owner" {
				owner = candidate
			}
		}
		if owner == "" {
			return Map{}, ErrNotFound
		}
		entry = &Map{ID: invite.MapID, Owner: owner, ACL: head.ACL, Snapshot: snapshot}
		s.maps[invite.MapID] = entry
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
		if _, err := store.CompareAndSwapHead(context.Background(), invite.MapID, head.Rev, protocol.Head{Rev: head.Rev, Seq: head.Seq + 1, BatchID: head.BatchID, SnapshotID: head.SnapshotID, SnapshotSeq: head.SnapshotSeq, ACL: acl}); err != nil {
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
