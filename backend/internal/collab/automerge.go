package collab

import (
	"errors"

	"github.com/automerge/automerge-go"
)

var errClosed = errors.New("collaboration document is closed")

type AutomergeDocument struct {
	doc *automerge.Doc
}

func New() Document { return &AutomergeDocument{doc: automerge.New()} }

func Load(data []byte) (Document, error) {
	doc, err := automerge.Load(data)
	if err != nil {
		return nil, err
	}
	return &AutomergeDocument{doc: doc}, nil
}

func (d *AutomergeDocument) Fork() (Document, error) {
	if d.doc == nil {
		return nil, errClosed
	}
	fork, err := d.doc.Fork()
	if err != nil {
		return nil, err
	}
	return &AutomergeDocument{doc: fork}, nil
}

func (d *AutomergeDocument) Apply(changes []byte) error {
	if d.doc == nil {
		return errClosed
	}
	return d.doc.LoadIncremental(changes)
}

func (d *AutomergeDocument) Save() ([]byte, error) {
	if d.doc == nil {
		return nil, errClosed
	}
	return d.doc.Save(), nil
}

func (d *AutomergeDocument) Heads() []string {
	if d.doc == nil {
		return nil
	}
	heads := d.doc.Heads()
	result := make([]string, len(heads))
	for i, head := range heads {
		result[i] = head.String()
	}
	return result
}

func (d *AutomergeDocument) MissingChanges(remoteHeads []string) ([]byte, error) {
	if d.doc == nil {
		return nil, errClosed
	}
	heads := make([]automerge.ChangeHash, 0, len(remoteHeads))
	for _, raw := range remoteHeads {
		head, err := automerge.NewChangeHash(raw)
		if err != nil {
			return nil, err
		}
		heads = append(heads, head)
	}
	changes, err := d.doc.Changes(heads...)
	if err != nil {
		return nil, err
	}
	return automerge.SaveChanges(changes), nil
}

func (d *AutomergeDocument) Close() { d.doc = nil }
