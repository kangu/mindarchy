package couch

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"

	"mindarchy/backend/internal/protocol"
)

var (
	ErrNotFound = errors.New("not_found")
	ErrConflict = errors.New("conflict")
)

type Store interface {
	ListMapIDs(context.Context) ([]protocol.MapID, error)
	LoadHead(context.Context, protocol.MapID) (protocol.Head, error)
	GetImmutable(context.Context, string) ([]byte, error)
	PutImmutable(context.Context, string, []byte) error
	CompareAndSwapHead(context.Context, protocol.MapID, string, protocol.Head) (protocol.Head, error)
}

type StoreClient struct {
	base   string
	db     string
	client *http.Client
	user   string
	pass   string
}

func (s *StoreClient) ListMapIDs(ctx context.Context) ([]protocol.MapID, error) {
	endpoint := s.base + "/" + url.PathEscape(s.db) + "/_all_docs?startkey=%22map:%22&endkey=%22map:%5Cuffff%22"
	request, err := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	if err != nil {
		return nil, err
	}
	if s.user != "" {
		request.SetBasicAuth(s.user, s.pass)
	}
	response, err := s.client.Do(request)
	if err != nil {
		return nil, err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("couch list status %d", response.StatusCode)
	}
	var result struct {
		Rows []struct {
			ID string `json:"id"`
		} `json:"rows"`
	}
	if err := json.NewDecoder(response.Body).Decode(&result); err != nil {
		return nil, err
	}
	ids := []protocol.MapID{}
	for _, row := range result.Rows {
		if strings.HasPrefix(row.ID, "map:") && strings.HasSuffix(row.ID, ":head") {
			ids = append(ids, protocol.MapID(strings.TrimSuffix(strings.TrimPrefix(row.ID, "map:"), ":head")))
		}
	}
	return ids, nil
}

func NewStore(baseURL, database, user, password string) *StoreClient {
	return &StoreClient{base: strings.TrimRight(baseURL, "/"), db: database, user: user, pass: password, client: &http.Client{Timeout: 15 * time.Second}}
}

func (s *StoreClient) Ping(ctx context.Context) error {
	request, err := http.NewRequestWithContext(ctx, http.MethodGet, s.base+"/"+url.PathEscape(s.db), nil)
	if err != nil {
		return err
	}
	if s.user != "" {
		request.SetBasicAuth(s.user, s.pass)
	}
	response, err := s.client.Do(request)
	if err != nil {
		return err
	}
	defer response.Body.Close()
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		return fmt.Errorf("couch ping status %d", response.StatusCode)
	}
	return nil
}

type document struct {
	ID      string `json:"_id"`
	Rev     string `json:"_rev,omitempty"`
	Kind    string `json:"kind"`
	Payload []byte `json:"payload"`
}

func (s *StoreClient) LoadHead(ctx context.Context, mapID protocol.MapID) (protocol.Head, error) {
	doc, err := s.get(ctx, headID(mapID))
	if err != nil {
		return protocol.Head{}, err
	}
	var head protocol.Head
	if err := json.Unmarshal(doc.Payload, &head); err != nil {
		return protocol.Head{}, fmt.Errorf("decode head: %w", err)
	}
	head.Rev = doc.Rev
	return head, nil
}

func (s *StoreClient) GetImmutable(ctx context.Context, id string) ([]byte, error) {
	doc, err := s.get(ctx, id)
	if err != nil {
		return nil, err
	}
	return append([]byte(nil), doc.Payload...), nil
}

func (s *StoreClient) PutImmutable(ctx context.Context, id string, payload []byte) error {
	if err := s.put(ctx, document{ID: id, Kind: "immutable", Payload: append([]byte(nil), payload...)}); err == nil {
		return nil
	} else if !errors.Is(err, ErrConflict) {
		return err
	}
	existing, err := s.GetImmutable(ctx, id)
	if err != nil {
		return err
	}
	if bytes.Equal(existing, payload) {
		return nil
	}
	return ErrConflict
}

func (s *StoreClient) CompareAndSwapHead(ctx context.Context, mapID protocol.MapID, expectedRev string, next protocol.Head) (protocol.Head, error) {
	payload, err := json.Marshal(next)
	if err != nil {
		return protocol.Head{}, err
	}
	doc := document{ID: headID(mapID), Rev: expectedRev, Kind: "head", Payload: payload}
	updated, err := s.putResponse(ctx, doc)
	if err != nil {
		return protocol.Head{}, err
	}
	next.Rev = updated.Rev
	return next, nil
}

func (s *StoreClient) get(ctx context.Context, id string) (document, error) {
	request, err := http.NewRequestWithContext(ctx, http.MethodGet, s.documentURL(id), nil)
	if err != nil {
		return document{}, err
	}
	if s.user != "" {
		request.SetBasicAuth(s.user, s.pass)
	}
	response, err := s.client.Do(request)
	if err != nil {
		return document{}, err
	}
	defer response.Body.Close()
	if response.StatusCode == http.StatusNotFound {
		return document{}, ErrNotFound
	}
	if response.StatusCode != http.StatusOK {
		return document{}, fmt.Errorf("couch get status %d", response.StatusCode)
	}
	var doc document
	if err := json.NewDecoder(io.LimitReader(response.Body, 64<<20)).Decode(&doc); err != nil {
		return document{}, err
	}
	return doc, nil
}

func (s *StoreClient) put(ctx context.Context, doc document) error {
	_, err := s.putResponse(ctx, doc)
	return err
}

func (s *StoreClient) putResponse(ctx context.Context, doc document) (document, error) {
	body, err := json.Marshal(doc)
	if err != nil {
		return document{}, err
	}
	request, err := http.NewRequestWithContext(ctx, http.MethodPut, s.documentURL(doc.ID), bytes.NewReader(body))
	if err != nil {
		return document{}, err
	}
	request.Header.Set("Content-Type", "application/json")
	if s.user != "" {
		request.SetBasicAuth(s.user, s.pass)
	}
	response, err := s.client.Do(request)
	if err != nil {
		return document{}, err
	}
	defer response.Body.Close()
	if response.StatusCode == http.StatusConflict {
		return document{}, ErrConflict
	}
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		return document{}, fmt.Errorf("couch put status %d", response.StatusCode)
	}
	var result struct {
		Rev string `json:"rev"`
	}
	if err := json.NewDecoder(response.Body).Decode(&result); err != nil {
		return document{}, err
	}
	doc.Rev = result.Rev
	return doc, nil
}

func (s *StoreClient) documentURL(id string) string {
	return s.base + "/" + url.PathEscape(s.db) + "/" + url.PathEscape(id)
}

func headID(id protocol.MapID) string { return "map:" + string(id) + ":head" }
