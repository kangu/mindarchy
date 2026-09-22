// mindarchy-maintenance applies explicit, per-map history retention.
package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"time"

	"mindarchy/backend/internal/config"
	"mindarchy/backend/internal/couch"
	"mindarchy/backend/internal/protocol"
	"mindarchy/backend/internal/rooms"
)

func main() {
	mapID := flag.String("map", "", "shared map ID (required)")
	action := flag.String("action", "checkpoint", "checkpoint or collect")
	keep := flag.Int("keep", 100, "number of recent full-state batches to retain")
	grace := flag.Duration("grace", 24*time.Hour, "delay before collection (minimum 1h)")
	apply := flag.Bool("apply", false, "perform the operation; default only inspects the head")
	timeout := flag.Duration("timeout", 10*time.Minute, "maintenance deadline")
	flag.Parse()
	if *mapID == "" || (*action != "checkpoint" && *action != "collect") {
		log.Fatal("provide -map and -action checkpoint|collect")
	}
	settings, err := config.FromEnv()
	if err != nil {
		log.Fatal(err)
	}
	store := couch.NewStore(settings.CouchURL, settings.CouchDatabase, settings.CouchUser, settings.CouchPassword)
	ctx, cancel := context.WithTimeout(context.Background(), *timeout)
	defer cancel()
	id := protocol.MapID(*mapID)
	head, err := store.LoadHead(ctx, id)
	if err != nil {
		log.Fatal(err)
	}
	if !*apply {
		fmt.Printf("Inspection only: seq=%d snapshotSeq=%d pendingCollection=%t. Use -apply to %s.\n", head.Seq, head.SnapshotSeq, head.GarbageID != "", *action)
		return
	}
	manager := rooms.NewManager(store)
	var count int
	if *action == "checkpoint" {
		count, err = manager.Checkpoint(ctx, id, *keep, *grace, time.Now().UTC())
	} else {
		count, err = manager.Collect(ctx, id, time.Now().UTC())
	}
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("%s: %d historical records scheduled or collected\n", *action, count)
}
