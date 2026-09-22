package liveops

import (
	"encoding/json"
	"reflect"
	"strings"
	"testing"
)

const uid = "12345678-1234-1234-1234-123456789abc"

func fixture(t *testing.T) Document {
	t.Helper()
	d, e := Normalize([]byte(`{"format":"mindarchy","version":1,"layout":"Horizontal","spacing":"Standard","branchStyle":"Rounded","manual":false,"nodes":[{"id":1,"parent":-1,"children":[8],"text":"Root","notes":"","folded":false,"task":false,"checked":false,"x":0,"y":0},{"id":8,"parent":1,"children":[],"text":"Child","notes":"","folded":false,"task":false,"checked":false,"x":0,"y":0}],"connections":[[1,8]]}`))
	if e != nil {
		t.Fatal(e)
	}
	return d
}
func TestRoundTrip(t *testing.T) {
	d := fixture(t)
	b, e := Project(d)
	if e != nil {
		t.Fatal(e)
	}
	again, e := Normalize(b)
	if e != nil || !reflect.DeepEqual(d, again) {
		t.Fatalf("roundtrip %s %v", b, e)
	}
	b2, _ := Project(d)
	if string(b) != string(b2) {
		t.Fatal("nondeterministic")
	}
}
func TestMergeAndImmutable(t *testing.T) {
	d := fixture(t)
	before, _ := json.Marshal(d)
	a, e := Apply(d, []Operation{{"set", []string{"nodes", "legacy:8", "text"}, "A"}})
	if e != nil {
		t.Fatal(e)
	}
	b, e := Apply(a, []Operation{{"set", []string{"nodes", "legacy:8", "notes"}, "B"}})
	if e != nil {
		t.Fatal(e)
	}
	n := b["nodes"].(map[string]any)["legacy:8"].(map[string]any)
	if n["text"] != "A" || n["notes"] != "B" {
		t.Fatal(n)
	}
	after, _ := json.Marshal(d)
	if string(before) != string(after) {
		t.Fatal("mutated input")
	}
}
func TestCreationDeletionAndMissingPatch(t *testing.T) {
	d := fixture(t)
	n := map[string]any{"parent": "legacy:8", "order": 0, "text": "new", "notes": "", "folded": false, "task": false, "checked": false, "x": 0, "y": 0}
	d, e := Apply(d, []Operation{{"set", []string{"nodes", uid}, n}})
	if e != nil {
		t.Fatal(e)
	}
	d, e = Apply(d, []Operation{{"remove", []string{"nodes", "legacy:8"}, nil}, {"set", []string{"nodes", uid, "text"}, "resurrect"}})
	if e != nil {
		t.Fatal(e)
	}
	if len(d["nodes"].(map[string]any)) != 1 || len(d["edges"].(map[string]any)) != 0 {
		t.Fatal(d)
	}
}
func TestAtomicInvalidOperations(t *testing.T) {
	for _, ops := range [][]Operation{
		{{"remove", []string{"nodes", "legacy:1"}, nil}},
		{{"set", []string{"nodes", "legacy:1", "parent"}, "legacy:8"}},
		{{"set", []string{"nodes", "legacy:8", "parent"}, "legacy:8"}},
		{{"set", []string{"nodes", "legacy:8", "parent"}, "absent"}},
		{{"set", []string{"nodes", "legacy:8"}, map[string]any{"text": "collision"}}},
		{{"remove", []string{"nodes", "legacy:8", "text"}, nil}},
		{{"set", []string{"props", "nodes"}, []any{}}},
		{{"set", []string{"nodes", "legacy:8", "text"}, strings.Repeat("x", 17000)}},
	} {
		d := fixture(t)
		before, _ := json.Marshal(d)
		if _, e := Apply(d, ops); e == nil {
			t.Fatalf("accepted %#v", ops)
		}
		after, _ := json.Marshal(d)
		if string(before) != string(after) {
			t.Fatal("mutated on rejection")
		}
	}
}
func TestNormalizeRejectsBrokenTree(t *testing.T) {
	b, _ := Project(fixture(t))
	var raw map[string]any
	json.Unmarshal(b, &raw)
	raw["nodes"].([]any)[0].(map[string]any)["children"] = []any{}
	b, _ = json.Marshal(raw)
	if _, e := Normalize(b); e == nil {
		t.Fatal("accepted inconsistent tree")
	}
}
func TestLimitsAndSettings(t *testing.T) {
	d := fixture(t)
	cases := []Operation{
		{"set", []string{"props", "extra"}, strings.Repeat("x", MaxDocumentBytes)},
		{"set", []string{"props", "themeId"}, "nonexistent"},
		{"set", []string{"nodes", "legacy:8", "calendar"}, map[string]any{"view": "bogus"}},
		{"set", []string{"nodes", "legacy:8", "meetingSection"}, "bogus"},
		{"set", []string{"nodes", "legacy:8", "style"}, map[string]any{"width": -1}},
	}
	for _, op := range cases {
		if _, e := Apply(d, []Operation{op}); e == nil {
			t.Fatalf("accepted invalid %#v", op.Path)
		}
	}
	if _, e := Apply(d, make([]Operation, MaxOperations+1)); e == nil {
		t.Fatal("operation limit")
	}
	if _, e := Normalize([]byte(strings.Repeat(" ", MaxDocumentBytes+1))); e == nil {
		t.Fatal("snapshot size")
	}
}
func TestUUIDCreationIdempotentAndValuesDetached(t *testing.T) {
	d := fixture(t)
	n := map[string]any{"parent": "legacy:1", "order": 0, "text": "new", "notes": "", "folded": false, "task": false, "checked": false, "x": 0, "y": 0}
	op := Operation{"set", []string{"nodes", uid}, n}
	got, e := Apply(d, []Operation{op})
	if e != nil {
		t.Fatal(e)
	}
	if _, e := Apply(got, []Operation{op}); e != nil {
		t.Fatal(e)
	}
	n["text"] = "mutated"
	if got["nodes"].(map[string]any)[uid].(map[string]any)["text"] != "new" {
		t.Fatal("shared operation value")
	}
}
func TestConcurrentChildrenDeterministic(t *testing.T) {
	d := fixture(t)
	newNode := func(text string) map[string]any {
		return map[string]any{"parent": "legacy:1", "order": 0, "text": text, "notes": "", "folded": false, "task": false, "checked": false, "x": 0, "y": 0}
	}
	a := Operation{"set", []string{"nodes", uid}, newNode("A")}
	b := Operation{"set", []string{"nodes", "22345678-1234-1234-1234-123456789abc"}, newNode("B")}
	first, e := Apply(d, []Operation{a, b})
	if e != nil {
		t.Fatal(e)
	}
	second, e := Apply(d, []Operation{b, a})
	if e != nil {
		t.Fatal(e)
	}
	x, _ := Project(first)
	y, _ := Project(second)
	if string(x) != string(y) {
		t.Fatal("order depended on creation delivery")
	}
}
