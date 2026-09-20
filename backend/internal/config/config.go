package config

import (
	"fmt"
	"net/url"
	"os"
	"time"
)

type Config struct {
	ListenAddr    string
	CouchURL      string
	CouchDatabase string
	CouchUser     string
	CouchPassword string
	OIDCIssuer    string
	OIDCAudience  string
	ReadTimeout   time.Duration
	WriteTimeout  time.Duration
	IdleTimeout   time.Duration
}

func FromEnv() (Config, error) {
	c := Config{
		ListenAddr:    env("MINDARCHY_LISTEN_ADDR", "127.0.0.1:8080"),
		CouchURL:      os.Getenv("MINDARCHY_COUCHDB_URL"),
		CouchDatabase: env("MINDARCHY_COUCHDB_DATABASE", "mindarchy-content"),
		CouchUser:     os.Getenv("MINDARCHY_COUCHDB_USER"),
		CouchPassword: os.Getenv("MINDARCHY_COUCHDB_PASSWORD"),
		OIDCIssuer:    os.Getenv("MINDARCHY_OIDC_ISSUER"),
		OIDCAudience:  os.Getenv("MINDARCHY_OIDC_AUDIENCE"),
		ReadTimeout:   duration("MINDARCHY_READ_TIMEOUT", 15*time.Second),
		WriteTimeout:  duration("MINDARCHY_WRITE_TIMEOUT", 15*time.Second),
		IdleTimeout:   duration("MINDARCHY_IDLE_TIMEOUT", 60*time.Second),
	}
	if c.CouchURL == "" {
		return Config{}, fmt.Errorf("MINDARCHY_COUCHDB_URL is required")
	}
	parsed, err := url.Parse(c.CouchURL)
	if err != nil || parsed.Scheme != "http" && parsed.Scheme != "https" || parsed.Host == "" {
		return Config{}, fmt.Errorf("MINDARCHY_COUCHDB_URL must be an absolute http(s) URL")
	}
	if c.CouchUser == "" || c.CouchPassword == "" {
		return Config{}, fmt.Errorf("CouchDB credentials are required")
	}
	if (c.OIDCIssuer == "") != (c.OIDCAudience == "") {
		return Config{}, fmt.Errorf("MINDARCHY_OIDC_ISSUER and MINDARCHY_OIDC_AUDIENCE must be set together")
	}
	return c, nil
}

func env(name, fallback string) string {
	if value := os.Getenv(name); value != "" {
		return value
	}
	return fallback
}
func duration(name string, fallback time.Duration) time.Duration {
	if value := os.Getenv(name); value != "" {
		if parsed, err := time.ParseDuration(value); err == nil && parsed > 0 {
			return parsed
		}
	}
	return fallback
}
