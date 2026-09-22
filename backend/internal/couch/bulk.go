package couch

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
)

// PutImmutables checks every per-document result. Bulk writes are not atomic;
// retrying a partial write verifies conflicting immutable values.
func (s *StoreClient) PutImmutables(ctx context.Context, values map[string][]byte) error {
	if len(values) == 0 {
		return nil
	}
	docs := make([]document, 0, len(values))
	for id, payload := range values {
		docs = append(docs, document{ID: id, Kind: "immutable", Payload: payload})
	}
	body, err := json.Marshal(struct {
		Docs []document `json:"docs"`
	}{docs})
	if err != nil {
		return err
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, s.base+"/"+url.PathEscape(s.db)+"/_bulk_docs", bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "application/json")
	if s.user != "" {
		req.SetBasicAuth(s.user, s.pass)
	}
	response, err := s.client.Do(req)
	if err != nil {
		return err
	}
	defer response.Body.Close()
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		return fmt.Errorf("couch bulk status %d", response.StatusCode)
	}
	var results []struct {
		ID    string `json:"id"`
		Rev   string `json:"rev"`
		Error string `json:"error"`
	}
	if err := json.NewDecoder(io.LimitReader(response.Body, 4<<20)).Decode(&results); err != nil {
		return err
	}
	seen := map[string]bool{}
	for _, result := range results {
		expected, ok := values[result.ID]
		if !ok || seen[result.ID] {
			return fmt.Errorf("unexpected bulk result")
		}
		seen[result.ID] = true
		switch result.Error {
		case "":
			if result.Rev == "" {
				return fmt.Errorf("missing bulk revision")
			}
		case "conflict":
			existing, err := s.GetImmutable(ctx, result.ID)
			if err != nil {
				return err
			}
			if !bytes.Equal(existing, expected) {
				return ErrConflict
			}
		default:
			return fmt.Errorf("couch bulk document error: %s", result.Error)
		}
	}
	if len(seen) != len(values) {
		return fmt.Errorf("incomplete bulk results")
	}
	return nil
}
