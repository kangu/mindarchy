package protocol

import (
	"bytes"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"regexp"
)

var uuidPattern = regexp.MustCompile(`^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$`)
var hashPattern = regexp.MustCompile(`^[0-9a-f]{64}$`)

var submitFields = map[string]bool{
	"version": true, "mapId": true, "deviceId": true, "counter": true, "hash": true, "changes": true,
}

// DecodeSubmit validates the JSON envelope before native collaboration data is decoded.
// The authenticated account is deliberately not part of this message.
func DecodeSubmit(data []byte) (Submit, error) {
	var fields map[string]json.RawMessage
	if err := decodeObject(data, &fields); err != nil {
		return Submit{}, invalid(err.Error())
	}
	for name := range fields {
		if !submitFields[name] {
			return Submit{}, invalid("unknown field " + name)
		}
	}
	for _, name := range []string{"version", "mapId", "deviceId", "counter", "hash", "changes"} {
		if _, ok := fields[name]; !ok {
			return Submit{}, invalid("missing field " + name)
		}
	}

	var value struct {
		Version  int    `json:"version"`
		MapID    string `json:"mapId"`
		DeviceID string `json:"deviceId"`
		Counter  uint64 `json:"counter"`
		Hash     string `json:"hash"`
		Changes  string `json:"changes"`
	}
	if err := strictUnmarshal(data, &value); err != nil {
		return Submit{}, invalid(err.Error())
	}
	if value.Version != Version {
		return Submit{}, &ProtocolError{Code: ErrorUnsupported, Details: fmt.Sprintf("version %d", value.Version)}
	}
	if !validUUID(value.MapID) || !validUUID(value.DeviceID) {
		return Submit{}, invalid("mapId and deviceId must be UUIDs")
	}
	if value.Counter == 0 {
		return Submit{}, invalid("counter must be greater than zero")
	}
	if !hashPattern.MatchString(value.Hash) {
		return Submit{}, invalid("hash must be lowercase SHA-256 hex")
	}
	if len(value.Changes) > MaxEncodedChanges {
		return Submit{}, &ProtocolError{Code: ErrorTooLarge, Details: "encoded changes exceed limit"}
	}
	changes, err := base64.StdEncoding.DecodeString(value.Changes)
	if err != nil || len(changes) == 0 {
		return Submit{}, invalid("changes must be non-empty base64")
	}
	if len(changes) > MaxDecodedChanges {
		return Submit{}, &ProtocolError{Code: ErrorTooLarge, Details: "decoded changes exceed limit"}
	}
	digest := sha256.Sum256(changes)
	if hex.EncodeToString(digest[:]) != value.Hash {
		return Submit{}, invalid("hash does not match changes")
	}
	return Submit{Version: value.Version, MapID: MapID(value.MapID), DeviceID: DeviceID(value.DeviceID), Counter: value.Counter, Hash: value.Hash, Changes: changes}, nil
}

func decodeObject(data []byte, target *map[string]json.RawMessage) error {
	decoder := json.NewDecoder(bytes.NewReader(data))
	first, err := decoder.Token()
	if err != nil {
		return err
	}
	delim, ok := first.(json.Delim)
	if !ok || delim != '{' {
		return fmt.Errorf("message must be a JSON object")
	}
	fields := make(map[string]json.RawMessage)
	for decoder.More() {
		key, err := decoder.Token()
		if err != nil {
			return err
		}
		name, ok := key.(string)
		if !ok {
			return fmt.Errorf("object key must be a string")
		}
		if _, exists := fields[name]; exists {
			return fmt.Errorf("duplicate field %s", name)
		}
		var value json.RawMessage
		if err := decoder.Decode(&value); err != nil {
			return err
		}
		fields[name] = value
	}
	if _, err := decoder.Token(); err != nil {
		return err
	}
	var trailing any
	if err := decoder.Decode(&trailing); err != io.EOF {
		return fmt.Errorf("trailing JSON")
	}
	*target = fields
	return nil
}

func strictUnmarshal(data []byte, target any) error {
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(target); err != nil {
		return err
	}
	var extra any
	if err := decoder.Decode(&extra); err != io.EOF {
		return fmt.Errorf("trailing JSON")
	}
	return nil
}

func validUUID(value string) bool {
	return uuidPattern.MatchString(value) && value != "00000000-0000-0000-0000-000000000000"
}
func invalid(details string) error {
	return &ProtocolError{Code: ErrorInvalidMessage, Details: details}
}
