package couch

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"mindarchy/backend/internal/protocol"
)

func TestStoreImmutableIdempotenceAndHeadCAS(t *testing.T) {
	docs := map[string]document{}
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		id := strings.TrimPrefix(r.URL.Path, "/db/")
		switch r.Method {
		case http.MethodGet:
			doc, ok := docs[id]
			if !ok {
				http.NotFound(w, r)
				return
			}
			_ = json.NewEncoder(w).Encode(doc)
		case http.MethodPut:
			var doc document
			if err := json.NewDecoder(r.Body).Decode(&doc); err != nil {
				http.Error(w, "bad", 400)
				return
			}
			old, exists := docs[id]
			if exists && doc.Rev != old.Rev {
				http.Error(w, "conflict", http.StatusConflict)
				return
			}
			doc.Rev = "1-x"
			docs[id] = doc
			_ = json.NewEncoder(w).Encode(map[string]string{"rev": doc.Rev})
		}
	}))
	defer server.Close()
	store := NewStore(server.URL, "db", "", "")
	ctx := context.Background()
	if err := store.PutImmutable(ctx, "batch:one", []byte("payload")); err != nil {
		t.Fatal(err)
	}
	if err := store.PutImmutable(ctx, "batch:one", []byte("payload")); err != nil {
		t.Fatal(err)
	}
	if err := store.PutImmutable(ctx, "batch:one", []byte("other")); err != ErrConflict {
		t.Fatalf("conflict = %v", err)
	}
	head, err := store.CompareAndSwapHead(ctx, protocol.MapID("map"), "", protocol.Head{Seq: 1})
	if err != nil {
		t.Fatal(err)
	}
	if head.Rev == "" {
		t.Fatal("head revision missing")
	}
	if _, err := store.CompareAndSwapHead(ctx, protocol.MapID("map"), "stale", protocol.Head{Seq: 2}); err != ErrConflict {
		t.Fatalf("stale CAS = %v", err)
	}
}
