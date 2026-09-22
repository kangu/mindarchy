package protocol

import (
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"errors"
	"fmt"
	"strings"
	"testing"
)

func validSubmit() string {
	data := []byte(`{"op":"insert","text":"hello"}`)
	digest := sha256.Sum256(data)
	return fmt.Sprintf(`{"version":1,"mapId":"a6e7db7b-81a6-43e2-a1cf-25421f4f81e1","deviceId":"1dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65","counter":1,"hash":"%s","changes":"%s"}`,
		hex.EncodeToString(digest[:]), base64.StdEncoding.EncodeToString(data))
}

func validHash() string {
	data := []byte(`{"op":"insert","text":"hello"}`)
	digest := sha256.Sum256(data)
	return hex.EncodeToString(digest[:])
}

func TestDecodeSubmit(t *testing.T) {
	tests := []struct {
		name, input, code string
	}{
		{"valid", validSubmit(), ""},
		{"unsupported version", strings.Replace(validSubmit(), `"version":1`, `"version":2`, 1), ErrorUnsupported},
		{"duplicate field", strings.Replace(validSubmit(), `"version":1`, `"version":1,"version":1`, 1), ErrorInvalidMessage},
		{"unknown field", strings.Replace(validSubmit(), `"counter":1`, `"account":"forged","counter":1`, 1), ErrorInvalidMessage},
		{"missing field", strings.Replace(validSubmit(), `"counter":1,`, "", 1), ErrorInvalidMessage},
		{"malformed id", strings.Replace(validSubmit(), "a6e7db7b-81a6-43e2-a1cf-25421f4f81e1", "not-an-id", 1), ErrorInvalidMessage},
		{"zero counter", strings.Replace(validSubmit(), `"counter":1`, `"counter":0`, 1), ErrorInvalidMessage},
		{"hash mismatch", strings.Replace(validSubmit(), validHash(), strings.Repeat("0", 64), 1), ErrorInvalidMessage},
		{"empty changes", strings.Replace(validSubmit(), base64.StdEncoding.EncodeToString([]byte(`{"op":"insert","text":"hello"}`)), "", 1), ErrorInvalidMessage},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			_, err := DecodeSubmit([]byte(test.input))
			if test.code == "" {
				if err != nil {
					t.Fatalf("DecodeSubmit() error = %v", err)
				}
				return
			}
			var protocolErr *ProtocolError
			if !errors.As(err, &protocolErr) || protocolErr.Code != test.code {
				t.Fatalf("error = %v, want code %q", err, test.code)
			}
		})
	}
}

func TestDecodeSubmitRejectsSizeLimit(t *testing.T) {
	data := make([]byte, MaxDecodedChanges+1)
	digest := sha256.Sum256(data)
	input := fmt.Sprintf(`{"version":1,"mapId":"a6e7db7b-81a6-43e2-a1cf-25421f4f81e1","deviceId":"1dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65","counter":1,"hash":"%s","changes":"%s"}`,
		hex.EncodeToString(digest[:]), base64.StdEncoding.EncodeToString(data))
	_, err := DecodeSubmit([]byte(input))
	var protocolErr *ProtocolError
	if !errors.As(err, &protocolErr) || protocolErr.Code != ErrorTooLarge {
		t.Fatalf("error = %v, want %s", err, ErrorTooLarge)
	}
}

func FuzzDecodeSubmit(f *testing.F) {
	f.Add([]byte(validSubmit()))
	f.Add([]byte(`{"version":1,"mapId":"bad"}`))
	f.Fuzz(func(t *testing.T, data []byte) {
		_, _ = DecodeSubmit(data)
	})
}
