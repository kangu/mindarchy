package couch

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
)

func TestImmutableBulkChecksEveryResult(t *testing.T) {
	partial := false
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/db/_bulk_docs" || r.Method != "POST" {
			t.Errorf("unexpected endpoint %s", r.URL)
			http.Error(w, "bad", 500)
			return
		}
		var body struct {
			Docs []document `json:"docs"`
		}
		if json.NewDecoder(r.Body).Decode(&body) != nil || len(body.Docs) != 2 {
			t.Fatal("wrong batch")
		}
		result := []map[string]string{}
		for _, d := range body.Docs {
			entry := map[string]string{"id": d.ID, "rev": "1-test"}
			if partial && d.ID == "b" {
				entry = map[string]string{"id": d.ID, "error": "forbidden"}
			}
			result = append(result, entry)
		}
		w.WriteHeader(201)
		json.NewEncoder(w).Encode(result)
	}))
	defer server.Close()
	store := NewStore(server.URL, "db", "", "")
	values := map[string][]byte{"a": []byte(`one`), "b": []byte(`two`)}
	if err := store.PutImmutables(context.Background(), values); err != nil {
		t.Fatal(err)
	}
	partial = true
	if err := store.PutImmutables(context.Background(), values); err == nil {
		t.Fatal("partial write failure ignored")
	}
}

func TestImmutableBulkConflictVerifiesExactPayload(t *testing.T) {
	existing := []byte("same")
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method == http.MethodPost {
			w.WriteHeader(201)
			json.NewEncoder(w).Encode([]map[string]string{{"id": "a", "error": "conflict"}})
			return
		}
		json.NewEncoder(w).Encode(document{ID: "a", Kind: "immutable", Payload: existing})
	}))
	defer server.Close()
	store := NewStore(server.URL, "db", "", "")
	if err := store.PutImmutables(context.Background(), map[string][]byte{"a": []byte("same")}); err != nil {
		t.Fatal(err)
	}
	existing = []byte("different")
	if err := store.PutImmutables(context.Background(), map[string][]byte{"a": []byte("same")}); err != ErrConflict {
		t.Fatalf("expected conflict, got %v", err)
	}
}
