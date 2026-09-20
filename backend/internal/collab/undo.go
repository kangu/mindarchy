package collab

import (
	"errors"
	"fmt"

	"github.com/automerge/automerge-go"
	"mindarchy/backend/internal/protocol"
)

var ErrUndoConflict = errors.New(protocol.ErrorUndoConflict)

type undoOperation struct {
	kind     string
	key      string
	before   string
	after    string
	position int
	inserted string
}

// UndoManager records this actor's semantic operations. It never restores a
// serialized document snapshot or writes over a changed current value.
type UndoManager struct {
	doc   *AutomergeDocument
	actor string
	undo  []undoOperation
	redo  []undoOperation
}

func NewUndoManager(doc Document) (*UndoManager, error) {
	automerged, ok := doc.(*AutomergeDocument)
	if !ok || automerged.doc == nil {
		return nil, errClosed
	}
	return &UndoManager{doc: automerged, actor: automerged.doc.ActorID()}, nil
}

func (m *UndoManager) ActorID() string { return m.actor }

func (m *UndoManager) SetString(key, value string) error {
	current, err := m.stringValue(key)
	if err != nil {
		return err
	}
	if current == value {
		return nil
	}
	if err := m.doc.doc.RootMap().Set(key, value); err != nil {
		return err
	}
	if _, err := m.doc.doc.Commit(fmt.Sprintf("set %s", key)); err != nil {
		return err
	}
	m.undo = append(m.undo, undoOperation{kind: "string", key: key, before: current, after: value})
	m.redo = nil
	return nil
}

func (m *UndoManager) InsertText(key string, position int, text string) error {
	if text == "" {
		return nil
	}
	current, err := m.textValue(key)
	if err != nil {
		return err
	}
	if position < 0 || position > len([]rune(current)) {
		return fmt.Errorf("text position %d out of range", position)
	}
	if err := m.doc.doc.Path(key).Text().Insert(position, text); err != nil {
		return err
	}
	if _, err := m.doc.doc.Commit(fmt.Sprintf("insert text in %s", key)); err != nil {
		return err
	}
	m.undo = append(m.undo, undoOperation{kind: "text-insert", key: key, before: current, after: insertRunes(current, position, text), position: position, inserted: text})
	m.redo = nil
	return nil
}

func (m *UndoManager) Undo() error {
	if len(m.undo) == 0 {
		return nil
	}
	op := m.undo[len(m.undo)-1]
	if err := m.apply(op, true); err != nil {
		return err
	}
	m.undo = m.undo[:len(m.undo)-1]
	m.redo = append(m.redo, op)
	return nil
}

func (m *UndoManager) Redo() error {
	if len(m.redo) == 0 {
		return nil
	}
	op := m.redo[len(m.redo)-1]
	if err := m.apply(op, false); err != nil {
		return err
	}
	m.redo = m.redo[:len(m.redo)-1]
	m.undo = append(m.undo, op)
	return nil
}

func (m *UndoManager) apply(op undoOperation, inverse bool) error {
	switch op.kind {
	case "string":
		current, err := m.stringValue(op.key)
		if err != nil {
			return err
		}
		expected, next := op.after, op.before
		if !inverse {
			expected, next = op.before, op.after
		}
		if current != expected {
			return ErrUndoConflict
		}
		if err := m.doc.doc.RootMap().Set(op.key, next); err != nil {
			return err
		}
	case "text-insert":
		current, err := m.textValue(op.key)
		if err != nil {
			return err
		}
		if inverse {
			if current != op.after || !hasRunesAt(current, op.position, op.inserted) {
				return ErrUndoConflict
			}
			if err := m.doc.doc.Path(op.key).Text().Delete(op.position, len([]rune(op.inserted))); err != nil {
				return err
			}
		} else {
			if current != op.before {
				return ErrUndoConflict
			}
			if err := m.doc.doc.Path(op.key).Text().Insert(op.position, op.inserted); err != nil {
				return err
			}
		}
	default:
		return fmt.Errorf("unknown undo operation %q", op.kind)
	}
	if _, err := m.doc.doc.Commit("undo/redo"); err != nil {
		return err
	}
	return nil
}

func (m *UndoManager) stringValue(key string) (string, error) {
	value, err := m.doc.doc.RootMap().Get(key)
	if err != nil {
		return "", err
	}
	if value.IsVoid() {
		return "", nil
	}
	if value.Kind() != automerge.KindStr {
		return "", fmt.Errorf("%s is not a string", key)
	}
	return value.Str(), nil
}

func (m *UndoManager) textValue(key string) (string, error) {
	return m.doc.doc.Path(key).Text().Get()
}

func hasRunesAt(value string, position int, expected string) bool {
	runes := []rune(value)
	inserted := []rune(expected)
	if position < 0 || position+len(inserted) > len(runes) {
		return false
	}
	for i, r := range inserted {
		if runes[position+i] != r {
			return false
		}
	}
	return true
}

func insertRunes(value string, position int, inserted string) string {
	runes := []rune(value)
	addition := []rune(inserted)
	result := make([]rune, 0, len(runes)+len(addition))
	result = append(result, runes[:position]...)
	result = append(result, addition...)
	result = append(result, runes[position:]...)
	return string(result)
}
