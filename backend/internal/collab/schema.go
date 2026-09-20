package collab

import (
	"fmt"
	"sort"
)

type Node struct {
	ID       string   `json:"id"`
	Parent   string   `json:"parent"`
	Children []string `json:"children"`
	Deleted  bool     `json:"deleted"`
	Kind     string   `json:"kind"`
	Title    string   `json:"title"`
	Notes    string   `json:"notes"`
}

type Relationship struct {
	ID   string `json:"id"`
	From string `json:"from"`
	To   string `json:"to"`
}

type Model struct {
	Root          string          `json:"root"`
	Nodes         map[string]Node `json:"nodes"`
	Relationships []Relationship  `json:"relationships"`
}

type Projection struct {
	Root          string          `json:"root"`
	Nodes         map[string]Node `json:"nodes"`
	Relationships []Relationship  `json:"relationships"`
}

func Project(model Model) (Projection, error) {
	if model.Root == "" {
		return Projection{}, fmt.Errorf("root is required")
	}
	root, ok := model.Nodes[model.Root]
	if !ok || root.Deleted || root.Parent != "" {
		return Projection{}, fmt.Errorf("root must be one live parentless node")
	}
	if err := validateNodes(model); err != nil {
		return Projection{}, err
	}

	parents := make(map[string]string, len(model.Nodes))
	for id, node := range model.Nodes {
		if id == model.Root || node.Deleted {
			continue
		}
		parent := node.Parent
		candidate, exists := model.Nodes[parent]
		if parent == "" || parent == id || !exists || candidate.Deleted {
			parent = model.Root
		}
		parents[id] = parent
	}
	for _, cycle := range cycles(parents, model.Root) {
		minimum := cycle[0]
		for _, id := range cycle[1:] {
			if id < minimum {
				minimum = id
			}
		}
		parents[minimum] = model.Root
	}

	projected := make(map[string]Node, len(model.Nodes))
	for id, node := range model.Nodes {
		if node.Deleted {
			continue
		}
		node.Children = nil
		if id != model.Root {
			node.Parent = parents[id]
		}
		projected[id] = node
	}
	for parentID, parent := range projected {
		if parentID == model.Root || parentID != model.Root && parent.Parent != "" {
			_ = parent
		}
		ordered := orderChildren(parentID, model.Nodes[parentID].Children, projected, parents)
		parent.Children = ordered
		projected[parentID] = parent
	}

	relations := make([]Relationship, 0, len(model.Relationships))
	for _, relation := range model.Relationships {
		if _, ok := projected[relation.From]; !ok {
			continue
		}
		if _, ok := projected[relation.To]; !ok {
			continue
		}
		relations = append(relations, relation)
	}
	return Projection{Root: model.Root, Nodes: projected, Relationships: relations}, nil
}

func validateNodes(model Model) error {
	for id, node := range model.Nodes {
		if id == "" || node.ID != id {
			return fmt.Errorf("node identity mismatch for %q", id)
		}
		if node.Kind == "" {
			return fmt.Errorf("node %q has no kind", id)
		}
		for _, child := range node.Children {
			if child == "" {
				return fmt.Errorf("node %q has an empty child reference", id)
			}
		}
	}
	return nil
}

func cycles(parents map[string]string, root string) [][]string {
	result := [][]string{}
	ids := make([]string, 0, len(parents))
	for id := range parents {
		ids = append(ids, id)
	}
	sort.Strings(ids)
	for _, start := range ids {
		seen := map[string]int{}
		path := []string{}
		current := start
		for current != root {
			if index, ok := seen[current]; ok {
				result = append(result, append([]string(nil), path[index:]...))
				break
			}
			seen[current] = len(path)
			path = append(path, current)
			next, ok := parents[current]
			if !ok {
				break
			}
			current = next
		}
	}
	return result
}

func orderChildren(parent string, references []string, nodes map[string]Node, parents map[string]string) []string {
	ordered := []string{}
	seen := map[string]bool{}
	for _, child := range references {
		if seen[child] || parents[child] != parent {
			continue
		}
		if _, ok := nodes[child]; ok {
			seen[child] = true
			ordered = append(ordered, child)
		}
	}
	unlisted := []string{}
	for child, effectiveParent := range parents {
		if effectiveParent == parent && !seen[child] {
			unlisted = append(unlisted, child)
		}
	}
	sort.Strings(unlisted)
	return append(ordered, unlisted...)
}
