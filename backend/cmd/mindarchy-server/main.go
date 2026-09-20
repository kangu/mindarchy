package main

import (
	"context"
	"errors"
	"log"
	"net/http"
	"os/signal"
	"syscall"

	"mindarchy/backend/internal/auth"
	"mindarchy/backend/internal/config"
	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/httpapi"
	"mindarchy/backend/internal/rooms"
	"mindarchy/backend/internal/sharing"
)

func main() {
	settings, err := config.FromEnv()
	if err != nil {
		log.Fatal(err)
	}
	store := couch.NewStore(settings.CouchURL, settings.CouchDatabase, settings.CouchUser, settings.CouchPassword)
	var verifier *auth.Verifier
	if settings.OIDCIssuer != "" {
		verifier, err = auth.NewVerifier(context.Background(), settings.OIDCIssuer, settings.OIDCAudience)
		if err != nil {
			log.Fatal(err)
		}
	}
	session := auth.NewCouchSession(settings.CouchURL)
	server := &http.Server{
		Addr: settings.ListenAddr, Handler: httpapi.NewProductionServer(store.Ping, verifier, sharing.NewPersistentService(store), session, rooms.NewManager(store)).Handler(),
		ReadTimeout: settings.ReadTimeout, WriteTimeout: settings.WriteTimeout, IdleTimeout: settings.IdleTimeout,
	}
	stop, stopSignal := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer stopSignal()
	go func() {
		<-stop.Done()
		shutdown, cancel := context.WithTimeout(context.Background(), settings.WriteTimeout)
		defer cancel()
		_ = server.Shutdown(shutdown)
	}()
	log.Printf("mindarchy server listening on %s", settings.ListenAddr)
	if err := server.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
		log.Fatal(err)
	}
}
