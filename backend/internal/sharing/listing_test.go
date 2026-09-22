package sharing

import (
	"context"
	"testing"
)

func TestListPageOnlyReturnsAccountSummaries(t *testing.T) {
	svc := NewService()
	first := svc.Create("owner", []byte(`{"private":"large contents"}`), "First")
	svc.Create("other", []byte(`{}`), "Private")
	second := svc.Create("owner", []byte(`{}`), "Second")
	page, next, err := svc.ListPage(context.Background(), "owner", "", 1)
	if err != nil || len(page) != 1 || next == "" {
		t.Fatalf("first page %v %s %v", page, next, err)
	}
	tail, end, err := svc.ListPage(context.Background(), "owner", next, 1)
	if err != nil || len(tail) != 1 || end != "" {
		t.Fatalf("second page %v %s %v", tail, end, err)
	}
	if page[0].ID == tail[0].ID {
		t.Fatal("duplicate page")
	}
	if (page[0].ID != first.ID && page[0].ID != second.ID) || (tail[0].ID != first.ID && tail[0].ID != second.ID) {
		t.Fatal("leaked other account map")
	}
	if _, _, err := svc.ListPage(context.Background(), "owner", "", 0); err == nil {
		t.Fatal("accepted unbounded page")
	}
}
