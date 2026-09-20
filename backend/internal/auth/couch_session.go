package auth

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"time"

	"mindarchy/backend/internal/protocol"
)

type CouchSession struct {
	base   string
	client *http.Client
}

func NewCouchSession(base string) *CouchSession {
	return &CouchSession{base: strings.TrimRight(base, "/"), client: &http.Client{Timeout: 10 * time.Second}}
}

type sessionReply struct {
	OK    bool     `json:"ok"`
	Name  string   `json:"name"`
	Roles []string `json:"roles"`
}

func (s *CouchSession) Login(ctx context.Context, username, password string) (Identity, string, error) {
	body, err := json.Marshal(map[string]string{"name": username, "password": password})
	if err != nil {
		return Identity{}, "", err
	}
	request, err := http.NewRequestWithContext(ctx, http.MethodPost, s.base+"/_session", bytes.NewReader(body))
	if err != nil {
		return Identity{}, "", err
	}
	request.Header.Set("Content-Type", "application/json")
	response, err := s.client.Do(request)
	if err != nil {
		return Identity{}, "", err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		return Identity{}, "", fmt.Errorf("couch session status %d", response.StatusCode)
	}
	var reply sessionReply
	if err := json.NewDecoder(response.Body).Decode(&reply); err != nil || !reply.OK || reply.Name == "" {
		return Identity{}, "", fmt.Errorf("invalid couch session")
	}
	cookie := ""
	for _, candidate := range response.Cookies() {
		if candidate.Name == "AuthSession" {
			cookie = candidate.Value
		}
	}
	if cookie == "" {
		return Identity{}, "", fmt.Errorf("CouchDB did not return AuthSession cookie")
	}
	return s.identity(reply.Name), cookie, nil
}

func (s *CouchSession) Verify(ctx context.Context, request *http.Request) (Identity, error) {
	cookie, err := request.Cookie("AuthSession")
	if err != nil || cookie.Value == "" {
		return Identity{}, fmt.Errorf("session cookie missing")
	}
	check, err := http.NewRequestWithContext(ctx, http.MethodGet, s.base+"/_session", nil)
	if err != nil {
		return Identity{}, err
	}
	check.AddCookie(cookie)
	response, err := s.client.Do(check)
	if err != nil {
		return Identity{}, err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		return Identity{}, fmt.Errorf("session validation failed")
	}
	var reply sessionReply
	if err := json.NewDecoder(response.Body).Decode(&reply); err != nil || !reply.OK || reply.Name == "" {
		return Identity{}, fmt.Errorf("invalid session")
	}
	return s.identity(reply.Name), nil
}

func (s *CouchSession) identity(name string) Identity {
	digest := sha256.Sum256([]byte("couchdb\x00" + name))
	return Identity{Account: protocol.AccountID("acct_" + hex.EncodeToString(digest[:])), Issuer: "couchdb", Subject: name}
}
