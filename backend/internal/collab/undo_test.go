package collab

import (
	"errors"
	"testing"

	"github.com/automerge/automerge-go"
)

func TestUndoRedoScalarUsesCurrentValuePrecondition(t *testing.T) {
	doc := automerge.New()
	if err := doc.RootMap().Set("title", "before"); err != nil {
		t.Fatal(err)
	}
	if _, err := doc.Commit("initial"); err != nil {
		t.Fatal(err)
	}
	manager, err := NewUndoManager(&AutomergeDocument{doc: doc})
	if err != nil {
		t.Fatal(err)
	}
	if err := manager.SetString("title", "local"); err != nil {
		t.Fatal(err)
	}
	if err := manager.Undo(); err != nil {
		t.Fatal(err)
	}
	assertString(t, doc, "title", "before")
	if err := manager.Redo(); err != nil {
		t.Fatal(err)
	}
	assertString(t, doc, "title", "local")

	if err := doc.RootMap().Set("title", "peer"); err != nil {
		t.Fatal(err)
	}
	if _, err := doc.Commit("peer edit"); err != nil {
		t.Fatal(err)
	}
	if err := manager.Undo(); !errors.Is(err, ErrUndoConflict) {
		t.Fatalf("Undo() error = %v, want %v", err, ErrUndoConflict)
	}
	assertString(t, doc, "title", "peer")
}

func TestUndoRedoUnicodeTextInsertion(t *testing.T) {
	doc := automerge.New()
	if err := doc.RootMap().Set("body", automerge.NewText("A😀B")); err != nil {
		t.Fatal(err)
	}
	if _, err := doc.Commit("initial"); err != nil {
		t.Fatal(err)
	}
	manager, err := NewUndoManager(&AutomergeDocument{doc: doc})
	if err != nil {
		t.Fatal(err)
	}
	if err := manager.InsertText("body", 1, "左"); err != nil {
		t.Fatal(err)
	}
	assertText(t, doc, "A左😀B")
	if err := manager.Undo(); err != nil {
		t.Fatal(err)
	}
	assertText(t, doc, "A😀B")
	if err := manager.Redo(); err != nil {
		t.Fatal(err)
	}
	assertText(t, doc, "A左😀B")

	if err := doc.Path("body").Text().Insert(0, "peer"); err != nil {
		t.Fatal(err)
	}
	if _, err := doc.Commit("peer text"); err != nil {
		t.Fatal(err)
	}
	if err := manager.Undo(); !errors.Is(err, ErrUndoConflict) {
		t.Fatalf("Undo() error = %v, want %v", err, ErrUndoConflict)
	}
}

func assertString(t *testing.T, doc *automerge.Doc, key, want string) {
	t.Helper()
	value, err := doc.RootMap().Get(key)
	if err != nil {
		t.Fatal(err)
	}
	if got := value.Str(); got != want {
		t.Fatalf("%s = %q, want %q", key, got, want)
	}
}

func assertText(t *testing.T, doc *automerge.Doc, want string) {
	t.Helper()
	got, err := doc.Path("body").Text().Get()
	if err != nil {
		t.Fatal(err)
	}
	if got != want {
		t.Fatalf("body = %q, want %q", got, want)
	}
}
