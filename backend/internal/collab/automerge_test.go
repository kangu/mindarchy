package collab

import (
	"strings"
	"testing"
	"unicode/utf8"

	"github.com/automerge/automerge-go"
)

func TestAutomergeRoundTripAndIncrementalApply(t *testing.T) {
	left := New().(*AutomergeDocument)
	if err := left.doc.RootMap().Set("title", "left"); err != nil {
		t.Fatal(err)
	}
	if _, err := left.doc.Commit("left title"); err != nil {
		t.Fatal(err)
	}
	full, err := left.Save()
	if err != nil {
		t.Fatal(err)
	}
	right, err := Load(full)
	if err != nil {
		t.Fatal(err)
	}
	if len(right.Heads()) != 1 {
		t.Fatalf("right heads = %v, want one head", right.Heads())
	}

	if err := left.doc.RootMap().Set("title", "updated"); err != nil {
		t.Fatal(err)
	}
	if _, err := left.doc.Commit("updated title"); err != nil {
		t.Fatal(err)
	}
	changes := left.doc.SaveIncremental()
	if len(changes) == 0 {
		t.Fatal("SaveIncremental returned no changes")
	}
	if err := right.Apply(changes); err != nil {
		t.Fatal(err)
	}
	if got, want := left.Heads(), right.Heads(); len(got) != len(want) || got[0] != want[0] {
		t.Fatalf("heads diverged: left=%v right=%v", got, want)
	}
	if err := right.Apply(changes); err != nil {
		t.Fatalf("duplicate incremental apply: %v", err)
	}

	rightDoc := right.(*AutomergeDocument)
	value, err := rightDoc.doc.RootMap().Get("title")
	if err != nil {
		t.Fatal(err)
	}
	if got := value.Str(); got != "updated" {
		t.Fatalf("title = %q, want updated", got)
	}
}

func TestAutomergeMissingChanges(t *testing.T) {
	left := New().(*AutomergeDocument)
	if err := left.doc.RootMap().Set("title", "left"); err != nil {
		t.Fatal(err)
	}
	if _, err := left.doc.Commit("left title"); err != nil {
		t.Fatal(err)
	}
	initial, err := left.Save()
	if err != nil {
		t.Fatal(err)
	}
	right, err := Load(initial)
	if err != nil {
		t.Fatal(err)
	}
	base := right.Heads()
	if err := left.doc.RootMap().Set("notes", automerge.NewText("hello")); err != nil {
		t.Fatal(err)
	}
	if _, err := left.doc.Commit("notes"); err != nil {
		t.Fatal(err)
	}
	changes, err := left.MissingChanges(base)
	if err != nil {
		t.Fatal(err)
	}
	if err := right.Apply(changes); err != nil {
		t.Fatal(err)
	}
	if got, want := left.Heads(), right.Heads(); len(got) != len(want) || got[0] != want[0] {
		t.Fatalf("heads diverged after missing changes: left=%v right=%v", got, want)
	}
}

func TestAutomergeConcurrentUnicodeTextConverges(t *testing.T) {
	base := automerge.New()
	if err := base.RootMap().Set("body", automerge.NewText("A😀e\u0301B")); err != nil {
		t.Fatal(err)
	}
	if _, err := base.Commit("initial text"); err != nil {
		t.Fatal(err)
	}
	left, err := base.Fork()
	if err != nil {
		t.Fatal(err)
	}
	right, err := base.Fork()
	if err != nil {
		t.Fatal(err)
	}
	if err := left.Path("body").Text().Insert(1, "左"); err != nil {
		t.Fatal(err)
	}
	if _, err := left.Commit("left unicode insertion"); err != nil {
		t.Fatal(err)
	}
	if err := right.Path("body").Text().Insert(1, "右"); err != nil {
		t.Fatal(err)
	}
	if _, err := right.Commit("right unicode insertion"); err != nil {
		t.Fatal(err)
	}
	if _, err := left.Merge(right); err != nil {
		t.Fatal(err)
	}
	value, err := left.Path("body").Text().Get()
	if err != nil {
		t.Fatal(err)
	}
	for _, part := range []string{"A", "左", "右", "😀", "e\u0301", "B"} {
		if !strings.Contains(value, part) {
			t.Fatalf("merged text %q does not contain %q", value, part)
		}
	}
	if utf8.RuneCountInString(value) != 7 {
		t.Fatalf("merged text has %d codepoints, want 7: %q", utf8.RuneCountInString(value), value)
	}
}

func TestAutomergeChangeIdentitySupportsActorScopedCompensation(t *testing.T) {
	doc := automerge.New()
	if err := doc.RootMap().Set("title", "before"); err != nil {
		t.Fatal(err)
	}
	hash, err := doc.Commit("actor edit")
	if err != nil {
		t.Fatal(err)
	}
	change, err := doc.Change(hash)
	if err != nil {
		t.Fatal(err)
	}
	if change.ActorID() == "" || change.ActorSeq() != 1 || len(change.Hash()) == 0 {
		t.Fatalf("change identity incomplete: actor=%q seq=%d hash=%s", change.ActorID(), change.ActorSeq(), change.Hash())
	}
	if len(change.Save()) == 0 {
		t.Fatal("change has no serialized identity payload")
	}
}
