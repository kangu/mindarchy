// Package liveops merges field operations on a stable-identity mind map.
package liveops

import (
	"encoding/json"
	"fmt"
	"math"
	"reflect"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"
	"unicode/utf16"
)

type Document map[string]any
type Operation struct {
	Op    string   `json:"op"`
	Path  []string `json:"path"`
	Value any      `json:"value,omitempty"`
}

const MaxDocumentBytes = 1 << 20
const MaxNodes = 10000
const MaxOperations = 1000

var uuid = regexp.MustCompile(`^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$`)

func object(v any) (map[string]any, bool) { m, ok := v.(map[string]any); return m, ok }
func number(v any) (float64, bool) {
	switch n := v.(type) {
	case float64:
		return n, !math.IsNaN(n) && !math.IsInf(n, 0)
	case int:
		return float64(n), true
	case json.Number:
		f, e := n.Float64()
		return f, e == nil
	}
	return 0, false
}
func integer(v any, min, max float64) (int, bool) {
	f, ok := number(v)
	return int(f), ok && f >= min && f <= max && math.Trunc(f) == f
}
func clone(v any) (any, error) {
	b, e := json.Marshal(v)
	if e != nil {
		return nil, e
	}
	var out any
	e = json.Unmarshal(b, &out)
	return out, e
}
func bad(s string) error { return fmt.Errorf("liveops: %s", s) }

// Normalize validates a legacy .omm snapshot and replaces numeric identities with sync IDs.
func Normalize(snapshot []byte) (Document, error) {
	if len(snapshot) > MaxDocumentBytes {
		return nil, bad("document size limit exceeded")
	}
	var raw map[string]any
	if e := json.Unmarshal(snapshot, &raw); e != nil {
		return nil, e
	}
	records, ok := raw["nodes"].([]any)
	if !ok || len(records) == 0 || len(records) > MaxNodes {
		return nil, bad("invalid nodes")
	}
	ids := map[int]string{}
	nodes := map[string]any{}
	parents := map[int]int{}
	children := map[int][]int{}
	for _, v := range records {
		n, ok := object(v)
		if !ok {
			return nil, bad("invalid node")
		}
		id, ok := integer(n["id"], 1, 1e9)
		if !ok || ids[id] != "" {
			return nil, bad("invalid or duplicate id")
		}
		p, ok := integer(n["parent"], -1, 1e9)
		if !ok {
			return nil, bad("invalid parent")
		}
		u := "legacy:" + strconv.Itoa(id)
		if s, present := n["syncId"]; present {
			u, ok = s.(string)
			if !ok || u == "" || len(u) > 128 {
				return nil, bad("invalid syncId")
			}
		}
		if _, exists := nodes[u]; exists {
			return nil, bad("duplicate syncId")
		}
		ids[id] = u
		parents[id] = p
		cs, ok := n["children"].([]any)
		if !ok {
			return nil, bad("invalid children")
		}
		seen := map[int]bool{}
		for _, c := range cs {
			ci, ok := integer(c, 1, 1e9)
			if !ok || seen[ci] {
				return nil, bad("invalid child")
			}
			seen[ci] = true
			children[id] = append(children[id], ci)
		}
		delete(n, "id")
		delete(n, "children")
		delete(n, "syncId")
		n["order"] = float64(0)
		nodes[u] = n
	}
	if ids[1] == "" || parents[1] != -1 {
		return nil, bad("root 1 required")
	}
	referenced := map[int]bool{}
	for id, u := range ids {
		n := nodes[u].(map[string]any)
		p := parents[id]
		if id == 1 {
			n["parent"] = ""
		} else {
			if ids[p] == "" {
				return nil, bad("missing parent")
			}
			n["parent"] = ids[p]
		}
		for index, c := range children[id] {
			if ids[c] == "" || parents[c] != id || referenced[c] {
				return nil, bad("inconsistent children")
			}
			referenced[c] = true
			nodes[ids[c]].(map[string]any)["order"] = float64(index)
		}
	}
	for id := range ids {
		if id != 1 && !referenced[id] {
			return nil, bad("unlisted child")
		}
	}
	edges := map[string]any{}
	if v, exists := raw["connections"]; exists {
		cs, ok := v.([]any)
		if !ok {
			return nil, bad("invalid connections")
		}
		for _, v := range cs {
			pair, ok := v.([]any)
			if !ok || len(pair) != 2 {
				return nil, bad("invalid connection")
			}
			a, ao := integer(pair[0], 1, 1e9)
			b, bo := integer(pair[1], 1, 1e9)
			if !ao || !bo || ids[a] == "" || ids[b] == "" || a == b {
				return nil, bad("invalid connection endpoints")
			}
			from, to := ids[a], ids[b]
			if from > to {
				from, to = to, from
			}
			m, _ := object(edges[from])
			if m == nil {
				m = map[string]any{}
				edges[from] = m
			}
			if m[to] != nil {
				return nil, bad("duplicate connection")
			}
			m[to] = true
		}
	}
	delete(raw, "nodes")
	delete(raw, "connections")
	d := Document{"props": raw, "nodes": nodes, "edges": edges}
	if _, e := Project(d); e != nil {
		return nil, e
	}
	return d, nil
}

// Apply returns a separate validated document. A rejected batch never mutates its input.
func Apply(doc Document, ops []Operation) (Document, error) {
	if len(ops) > MaxOperations {
		return nil, bad("operation limit exceeded")
	}
	if _, e := Project(doc); e != nil {
		return nil, e
	}
	v, e := clone(doc)
	if e != nil {
		return nil, e
	}
	d := Document(v.(map[string]any))
	nodes := d["nodes"].(map[string]any)
	root, _ := validate(d)
	for _, op := range ops {
		p := op.Path
		if (op.Op != "set" && op.Op != "remove") || len(p) < 2 || len(p) > 32 {
			return nil, bad("invalid operation")
		}
		for _, s := range p {
			if s == "" || len(s) > 1024 {
				return nil, bad("invalid path")
			}
		}
		value, e := clone(op.Value)
		if e != nil {
			return nil, e
		}
		switch p[0] {
		case "nodes":
			if len(p) == 2 {
				if op.Op == "remove" {
					if p[1] == root {
						return nil, bad("cannot remove root")
					}
					removeTree(d, p[1])
					continue
				}
				n, ok := object(value)
				if !ok {
					return nil, bad("node creation must be object")
				}
				if old, exists := nodes[p[1]]; exists {
					if !reflect.DeepEqual(old, n) {
						return nil, bad("identity collision")
					}
					continue
				}
				if !uuid.MatchString(p[1]) || p[1] == "00000000-0000-0000-0000-000000000000" {
					return nil, bad("new node requires UUID")
				}
				nodes[p[1]] = n
				continue
			}
			if p[2] == "id" || p[2] == "children" || p[2] == "syncId" {
				return nil, bad("reserved node field")
			}
			n, exists := nodes[p[1]]
			if !exists {
				continue
			}
			if p[1] == root && p[2] == "parent" && (op.Op != "set" || value != "") {
				return nil, bad("cannot move root")
			}
			if e := patch(n.(map[string]any), p[2:], op.Op, value); e != nil {
				return nil, e
			}
		case "props":
			if p[1] == "nodes" || p[1] == "connections" {
				return nil, bad("reserved property")
			}
			if e := patch(d["props"].(map[string]any), p[1:], op.Op, value); e != nil {
				return nil, e
			}
		case "edges":
			if len(p) != 3 || p[1] == p[2] {
				return nil, bad("invalid edge path")
			}
			a, b := p[1], p[2]
			if a > b {
				a, b = b, a
			}
			edges := d["edges"].(map[string]any)
			m, _ := object(edges[a])
			if op.Op == "remove" {
				delete(m, b)
				if len(m) == 0 {
					delete(edges, a)
				}
				continue
			}
			if value != true {
				return nil, bad("edge value must be true")
			}
			if nodes[a] == nil || nodes[b] == nil {
				continue
			}
			if m == nil {
				m = map[string]any{}
				edges[a] = m
			}
			m[b] = true
		default:
			return nil, bad("invalid operation namespace")
		}
	}
	if _, e := Project(d); e != nil {
		return nil, e
	}
	return d, nil
}
func patch(m map[string]any, path []string, op string, value any) error {
	for _, key := range path[:len(path)-1] {
		next, exists := m[key]
		if !exists {
			if op == "remove" {
				return nil
			}
			child := map[string]any{}
			m[key] = child
			m = child
			continue
		}
		child, ok := object(next)
		if !ok {
			return bad("field path traverses non-object")
		}
		m = child
	}
	key := path[len(path)-1]
	if op == "remove" {
		delete(m, key)
	} else {
		m[key] = value
	}
	return nil
}
func removeTree(d Document, uid string) {
	nodes := d["nodes"].(map[string]any)
	children := map[string][]string{}
	for id, v := range nodes {
		n := v.(map[string]any)
		p, _ := n["parent"].(string)
		children[p] = append(children[p], id)
	}
	pending := []string{uid}
	gone := map[string]bool{}
	for len(pending) > 0 {
		id := pending[len(pending)-1]
		pending = pending[:len(pending)-1]
		if gone[id] {
			continue
		}
		gone[id] = true
		pending = append(pending, children[id]...)
		delete(nodes, id)
	}
	edges := d["edges"].(map[string]any)
	for from, v := range edges {
		if gone[from] {
			delete(edges, from)
			continue
		}
		m := v.(map[string]any)
		for to := range m {
			if gone[to] {
				delete(m, to)
			}
		}
		if len(m) == 0 {
			delete(edges, from)
		}
	}
}
func oneOf(v any, allowed ...string) bool {
	s, ok := v.(string)
	if !ok {
		return false
	}
	for _, a := range allowed {
		if s == a {
			return true
		}
	}
	return false
}
func validate(d Document) (string, error) {
	props, po := object(d["props"])
	nodes, no := object(d["nodes"])
	edges, eo := object(d["edges"])
	if !po || !no || !eo || len(d) != 3 || len(nodes) == 0 || len(nodes) > MaxNodes {
		return "", bad("invalid canonical document")
	}
	version, ok := integer(props["version"], 1, 1)
	if props["format"] != "mindarchy" || !ok || version != 1 || !oneOf(props["layout"], "Horizontal", "Vertical", "Compact") || !oneOf(props["spacing"], "Narrow", "Standard", "Wide") || !oneOf(props["branchStyle"], "Rounded", "Angular", "Botanical graphite", "Living oak", "Sumi branch", "Silver birch", "Elven filigree") {
		return "", bad("invalid document settings")
	}
	manual, ok := props["manual"].(bool)
	if !ok || (props["layout"] == "Compact" && manual) {
		return "", bad("invalid manual setting")
	}
	if _, ok := props["nodes"]; ok {
		return "", bad("reserved property")
	}
	if _, ok := props["connections"]; ok {
		return "", bad("reserved property")
	}
	if theme, exists := props["themeId"]; exists {
		if !oneOf(theme, "beach-day", "holographic", "retro", "arcade", "lab", "canopy", "atlas", "studio", "nocturne", "porcelain", "omarchy", "sky", "starlight", "sage", "blush", "graphite", "paper", "forest", "midnight") {
			return "", bad("invalid theme")
		}
	}
	root := ""
	media := mediaBudget{}
	parents := map[string]string{}
	for uid, v := range nodes {
		n, ok := object(v)
		if !ok || uid == "" || len(uid) > 128 {
			return "", bad("invalid node")
		}
		for _, key := range []string{"id", "children", "syncId"} {
			if _, exists := n[key]; exists {
				return "", bad("reserved node field")
			}
		}
		p, ok := n["parent"].(string)
		if !ok {
			return "", bad("invalid parent")
		}
		parents[uid] = p
		if p == "" {
			if root != "" {
				return "", bad("multiple roots")
			}
			root = uid
		} else if nodes[p] == nil {
			return "", bad("missing parent")
		}
		if _, ok := number(n["order"]); !ok {
			return "", bad("invalid order")
		}
		for _, key := range []string{"text", "notes"} {
			s, ok := n[key].(string)
			if !ok || len(utf16.Encode([]rune(s))) > 16384 {
				return "", bad("invalid text field")
			}
		}
		for _, key := range []string{"folded", "task", "checked"} {
			if _, ok := n[key].(bool); !ok {
				return "", bad("invalid boolean field")
			}
		}
		for _, key := range []string{"x", "y"} {
			f, ok := number(n[key])
			if !ok || math.Abs(f) > 1e6 {
				return "", bad("invalid position")
			}
		}
		if n["checked"] == true && n["task"] != true {
			return "", bad("checked requires task")
		}
		if kind, exists := n["kind"]; exists && !oneOf(kind, "text", "date") {
			return "", bad("invalid kind")
		}
		if n["kind"] == "date" && (n["task"] == true || n["checked"] == true) {
			return "", bad("date cannot be task")
		}
		for _, key := range []string{"style", "meeting", "calendar"} {
			if value, exists := n[key]; exists {
				if _, ok := object(value); !ok {
					return "", bad("invalid object field")
				}
			}
		}
		if e := validateMetadata(n); e != nil {
			return "", e
		}
		if e := validateMedia(n, &media); e != nil {
			return "", e
		}
	}
	if root == "" {
		return "", bad("root required")
	}
	depths := map[string]int{root: 0}
	for uid := range nodes {
		chain := []string{}
		seen := map[string]bool{}
		current := uid
		for {
			if _, known := depths[current]; known {
				break
			}
			if seen[current] || len(chain) > 512 {
				return "", bad("cycle or depth limit exceeded")
			}
			seen[current] = true
			chain = append(chain, current)
			current = parents[current]
		}
		depth := depths[current]
		for i := len(chain) - 1; i >= 0; i-- {
			depth++
			if depth > 512 {
				return "", bad("depth limit exceeded")
			}
			depths[chain[i]] = depth
		}
	}
	count := 0
	for from, v := range edges {
		m, ok := object(v)
		if !ok || nodes[from] == nil {
			return "", bad("invalid edge source")
		}
		for to, value := range m {
			count++
			if from >= to || nodes[to] == nil || value != true {
				return "", bad("invalid edge")
			}
		}
	}
	if count > MaxNodes {
		return "", bad("edge limit exceeded")
	}
	return root, nil
}

// Project deterministically rebuilds an Engine-compatible numeric-ID .omm document.
func Project(doc Document) ([]byte, error) {
	root, e := validate(doc)
	if e != nil {
		return nil, e
	}
	nodes := doc["nodes"].(map[string]any)
	uids := make([]string, 0, len(nodes))
	for uid := range nodes {
		if uid != root {
			uids = append(uids, uid)
		}
	}
	sort.Strings(uids)
	uids = append([]string{root}, uids...)
	ids := map[string]int{}
	children := map[string][]string{}
	for i, uid := range uids {
		ids[uid] = i + 1
		n := nodes[uid].(map[string]any)
		p := n["parent"].(string)
		children[p] = append(children[p], uid)
	}
	for p := range children {
		sort.Slice(children[p], func(i, j int) bool {
			a, b := children[p][i], children[p][j]
			oa, _ := number(nodes[a].(map[string]any)["order"])
			ob, _ := number(nodes[b].(map[string]any)["order"])
			if oa == ob {
				return a < b
			}
			return oa < ob
		})
	}
	out := map[string]any{}
	for k, v := range doc["props"].(map[string]any) {
		out[k] = v
	}
	records := make([]any, 0, len(uids))
	for _, uid := range uids {
		n := nodes[uid].(map[string]any)
		record := map[string]any{}
		for k, v := range n {
			if k != "parent" && k != "order" {
				record[k] = v
			}
		}
		record["id"] = ids[uid]
		record["syncId"] = uid
		record["parent"] = -1
		if uid != root {
			record["parent"] = ids[n["parent"].(string)]
		}
		cs := []int{}
		for _, child := range children[uid] {
			cs = append(cs, ids[child])
		}
		record["children"] = cs
		records = append(records, record)
	}
	out["nodes"] = records
	pairs := [][2]int{}
	for from, v := range doc["edges"].(map[string]any) {
		for to := range v.(map[string]any) {
			a, b := ids[from], ids[to]
			if a > b {
				a, b = b, a
			}
			pairs = append(pairs, [2]int{a, b})
		}
	}
	sort.Slice(pairs, func(i, j int) bool {
		if pairs[i][0] == pairs[j][0] {
			return pairs[i][1] < pairs[j][1]
		}
		return pairs[i][0] < pairs[j][0]
	})
	out["connections"] = pairs
	b, e := json.Marshal(out)
	if e != nil {
		return nil, e
	}
	if len(b) > MaxDocumentBytes {
		return nil, bad("document size limit exceeded")
	}
	return b, nil
}

func validDate(v any) bool {
	s, ok := v.(string)
	if !ok {
		return false
	}
	d, e := time.Parse("2006-01-02", s)
	return e == nil && d.Year() >= 1 && d.Year() <= 9999 && d.Format("2006-01-02") == s
}
func validateMetadata(n map[string]any) error {
	c, _ := object(n["calendar"])
	if n["kind"] == "date" || len(c) > 0 {
		if !oneOf(c["view"], "week", "month") || !validDate(c["anchor"]) {
			return bad("invalid calendar")
		}
		entries, ok := object(c["entries"])
		if !ok || len(entries) > 3660 {
			return bad("invalid calendar entries")
		}
		for day, value := range entries {
			s, ok := value.(string)
			if !validDate(day) || !ok || strings.TrimSpace(s) == "" || len(utf16.Encode([]rune(s))) > 4096 {
				return bad("invalid calendar entry")
			}
		}
	}
	if section, exists := n["meetingSection"]; exists && !oneOf(section, "", "agenda", "notes", "decisions", "actions") {
		return bad("invalid meeting section")
	}
	meeting, _ := object(n["meeting"])
	if len(meeting) > 0 {
		if (n["kind"] != nil && n["kind"] != "text") || !validDate(meeting["date"]) {
			return bad("invalid meeting")
		}
		if v, ok := meeting["time"]; ok && v != "" {
			s, ok := v.(string)
			if !ok {
				return bad("invalid meeting time")
			}
			if _, e := time.Parse("15:04", s); e != nil {
				return bad("invalid meeting time")
			}
		}
		if v, exists := meeting["attendees"]; exists {
			s, ok := v.(string)
			if !ok || len(utf16.Encode([]rune(s))) > 4096 {
				return bad("invalid attendees")
			}
		}
	}
	style, _ := object(n["style"])
	for key, value := range style {
		if key == "fill" || key == "border" || key == "branch" || key == "textColor" {
			s, ok := value.(string)
			if !ok || s == "" {
				return bad("invalid style color")
			}
			continue
		}
		f, ok := number(value)
		if !ok {
			return bad("invalid style number")
		}
		switch key {
		case "shape":
			ok = f >= 0 && f <= 7 && math.Trunc(f) == f
		case "borderStyle", "branchStroke":
			ok = f >= 1 && f <= 3 && math.Trunc(f) == f
		case "width":
			ok = f == 0 || (f >= 70 && f <= 1200)
		case "radius":
			ok = f >= 0 && f <= 1000
		case "borderWidth", "branchWidth":
			ok = f >= 0 && f <= 20
		default:
			ok = false
		}
		if !ok {
			return bad("invalid style field")
		}
	}
	return nil
}
