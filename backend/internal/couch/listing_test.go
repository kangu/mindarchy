package couch

import (
	"context"
	"encoding/json"
	"mindarchy/backend/internal/protocol"
	"net/http"
	"net/http/httptest"
	"os/exec"
	"testing"
)

func TestMembershipListingUsesAccountIndexAndCursor(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/db/_design/mindarchy-membership-v1/_view/by_account" {
			t.Errorf("unexpected scan: %s", r.URL)
			http.Error(w, "wrong path", 500)
			return
		}
		var start []any
		json.Unmarshal([]byte(r.URL.Query().Get("startkey")), &start)
		if len(start) != 3 || start[0] != "owner" || start[1] != "a" || r.URL.Query().Get("limit") != "3" || r.URL.Query().Get("skip") != "" {
			t.Errorf("query %s", r.URL.RawQuery)
		}
		json.NewEncoder(w).Encode(map[string]any{"rows": []any{
			map[string]any{"key": []string{"owner", "b"}, "value": MapSummary{ID: "b", Name: "B", Owner: "owner", ACL: map[string]string{"owner": "owner"}}},
			map[string]any{"key": []string{"owner", "c"}, "value": MapSummary{ID: "c", Name: "C"}},
			map[string]any{"key": []string{"owner", "d"}, "value": MapSummary{ID: "d", Name: "D"}},
		}})
	}))
	defer server.Close()
	page, next, err := NewStore(server.URL, "db", "", "").ListAccountMaps(context.Background(), "owner", "a", 2)
	if err != nil {
		t.Fatal(err)
	}
	if len(page) != 2 || next != "c" || page[0].ID != "b" {
		t.Fatalf("page %v next %s", page, next)
	}
}

func TestMembershipViewIndexesLegacyUnicodeAndNewHeads(t *testing.T) {
	node, err := exec.LookPath("node")
	if err != nil {
		t.Skip("node required to execute CouchDB map function")
	}
	head := protocol.Head{Name: "Plan 🌳 日本語", ACL: map[protocol.AccountID]string{"owner": "owner", "reader": "viewer"}}
	payload, _ := json.Marshal(head)
	docs := []document{{ID: "map:legacy:head", Kind: "head", Payload: payload}, {ID: "map:new:head", Kind: "head", Head: &head}, {ID: "map:other:batch:123", Kind: "immutable", Payload: []byte(`{}`)}}
	encoded, _ := json.Marshal(docs)
	script := `const rows=[]; function emit(key,value){rows.push({key,value})};const map=` + membershipMap + `;` + `const docs=` + string(encoded) + `;docs.forEach(map);process.stdout.write(JSON.stringify(rows));`
	output, err := exec.Command(node, "-e", script).CombinedOutput()
	if err != nil {
		t.Fatalf("map function: %v %s", err, output)
	}
	var rows []struct {
		Key   []string
		Value MapSummary
	}
	if err := json.Unmarshal(output, &rows); err != nil {
		t.Fatal(err)
	}
	if len(rows) != 4 {
		t.Fatalf("rows: %s", output)
	}
	for _, row := range rows {
		if row.Value.Name != head.Name || len(row.Value.ACL) != 1 || row.Value.ACL[row.Key[0]] == "" {
			t.Fatalf("incorrect or overbroad summary: %s", output)
		}
	}
}
