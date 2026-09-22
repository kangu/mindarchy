package collab

// Document is the project-owned boundary around the collaboration engine.
// Payloads passed to Apply and MissingChanges are Automerge incremental/change
// bytes, never materialized map JSON.
type Document interface {
	Fork() (Document, error)
	Apply(changes []byte) error
	Save() ([]byte, error)
	Heads() []string
	MissingChanges(remoteHeads []string) ([]byte, error)
	Close()
}
