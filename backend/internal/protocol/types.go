package protocol

import "fmt"

const (
	Version             = 1
	MaxDecodedChanges   = 1 << 20
	MaxEncodedChanges   = ((MaxDecodedChanges + 2) / 3) * 4
	ErrorUnsupported    = "unsupported_version"
	ErrorInvalidMessage = "invalid_message"
	ErrorTooLarge       = "too_large"
	ErrorCounterReuse   = "counter_reuse"
	ErrorMissingDeps    = "missing_dependencies"
	ErrorAccessRevoked  = "access_revoked"
	ErrorResync         = "resync_required"
	ErrorQuota          = "quota_exceeded"
	ErrorUndoConflict   = "undo_conflict"
)

// ProtocolError is safe to expose to clients. Details are for server logs only.
type ProtocolError struct {
	Code    string
	Details string
}

func (e *ProtocolError) Error() string {
	if e.Details == "" {
		return e.Code
	}
	return fmt.Sprintf("%s: %s", e.Code, e.Details)
}

type AccountID string
type MapID string
type DeviceID string

type Submit struct {
	Version  int      `json:"version"`
	MapID    MapID    `json:"mapId"`
	DeviceID DeviceID `json:"deviceId"`
	Counter  uint64   `json:"counter"`
	Hash     string   `json:"hash"`
	Changes  []byte   `json:"changes"`
}

type Receipt struct {
	DeviceID DeviceID `json:"deviceId"`
	Counter  uint64   `json:"counter"`
	Hash     string   `json:"hash"`
	Seq      uint64   `json:"seq"`
}

type Head struct {
	Rev         string               `json:"rev"`
	Seq         uint64               `json:"seq"`
	BatchID     string               `json:"batchId"`
	SnapshotID  string               `json:"snapshotId"`
	SnapshotSeq uint64               `json:"snapshotSeq"`
	ACL         map[AccountID]string `json:"acl"`
	Name        string               `json:"name,omitempty"`
	Deleted     bool                 `json:"deleted"`
}
