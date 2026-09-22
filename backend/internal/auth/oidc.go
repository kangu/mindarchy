package auth

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"net/http"
	"strings"

	"github.com/coreos/go-oidc/v3/oidc"
	"mindarchy/backend/internal/protocol"
)

type Identity struct {
	Account protocol.AccountID
	Issuer  string
	Subject string
}

type Verifier struct {
	issuer string
	verify *oidc.IDTokenVerifier
}

func NewVerifier(ctx context.Context, issuer, audience string) (*Verifier, error) {
	provider, err := oidc.NewProvider(ctx, issuer)
	if err != nil {
		return nil, fmt.Errorf("oidc provider: %w", err)
	}
	return &Verifier{issuer: issuer, verify: provider.Verifier(&oidc.Config{ClientID: audience})}, nil
}

func (v *Verifier) VerifyRequest(ctx context.Context, request *http.Request) (Identity, error) {
	header := request.Header.Get("Authorization")
	if !strings.HasPrefix(header, "Bearer ") {
		return Identity{}, fmt.Errorf("missing bearer token")
	}
	token, err := v.verify.Verify(ctx, strings.TrimSpace(strings.TrimPrefix(header, "Bearer ")))
	if err != nil {
		return Identity{}, fmt.Errorf("verify token: %w", err)
	}
	var claims struct {
		Subject string `json:"sub"`
	}
	if err := token.Claims(&claims); err != nil || claims.Subject == "" {
		return Identity{}, fmt.Errorf("token subject missing")
	}
	hash := sha256.Sum256([]byte(v.issuer + "\x00" + claims.Subject))
	return Identity{Account: protocol.AccountID("acct_" + hex.EncodeToString(hash[:])), Issuer: v.issuer, Subject: claims.Subject}, nil
}
