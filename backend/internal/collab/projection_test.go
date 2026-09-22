package collab

import "testing"

func TestProjectCycleDeletedParentsAndOrdering(t *testing.T) {
	model := Model{Root: "root", Nodes: map[string]Node{
		"root": {ID: "root", Kind: "root", Children: []string{"b", "b", "missing", "a"}},
		"a":    {ID: "a", Parent: "b", Kind: "text"},
		"b":    {ID: "b", Parent: "a", Kind: "text"},
		"c":    {ID: "c", Parent: "gone", Kind: "text"},
		"gone": {ID: "gone", Deleted: true, Kind: "text"},
	}, Relationships: []Relationship{{ID: "live", From: "root", To: "a"}, {ID: "hidden", From: "root", To: "gone"}}}
	projection, err := Project(model)
	if err != nil {
		t.Fatal(err)
	}
	if got := projection.Nodes["a"].Parent; got != "root" {
		t.Fatalf("a parent = %q, want root", got)
	}
	if got := projection.Nodes["b"].Parent; got != "a" {
		t.Fatalf("b parent = %q, want a", got)
	}
	if got := projection.Nodes["c"].Parent; got != "root" {
		t.Fatalf("orphan parent = %q, want root", got)
	}
	if got := projection.Nodes["root"].Children; len(got) != 2 || got[0] != "a" || got[1] != "c" {
		t.Fatalf("children = %v", got)
	}
	if len(projection.Relationships) != 1 || projection.Relationships[0].ID != "live" {
		t.Fatalf("relationships = %v", projection.Relationships)
	}
}

func TestProjectRejectsRootMutationAndIdentityMismatch(t *testing.T) {
	_, err := Project(Model{Root: "root", Nodes: map[string]Node{"root": {ID: "other", Kind: "root"}}})
	if err == nil {
		t.Fatal("identity mismatch accepted")
	}
	_, err = Project(Model{Root: "root", Nodes: map[string]Node{"root": {ID: "root", Parent: "other", Kind: "root"}}})
	if err == nil {
		t.Fatal("mutated root accepted")
	}
}
