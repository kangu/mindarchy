# Live WebSocket v2 protocol

## Initial Prompt
Proceed with websocket recommendations. Implement the protocol. Couchdb is running bare metal with no docker on the current machine.

## Plan
1. Define stable-node operations and version negotiation with separate live and durable acknowledgements.
2. Implement Go merge rooms, bounded batching, receipt deduplication and crash recovery.
3. Implement Qt durable outbox/reconciliation, stable selection and undo, legacy recovery.
4. Verify failures, simultaneous editing, permissions and restart against memory and local bare-metal CouchDB; review and resolve findings.
5. Build the app/server and document deployment constraints.

## Next Steps
- Rebuild/restart the deployed Go server and collaborating clients together. The existing running backend was not replaced.
- Run a single Go writer per database; stop it for maintenance. Do not downgrade promoted maps to legacy writers.
- Measure production room latency, bandwidth and disk growth; consider delta broadcasts and fenced room routing only after those measurements.

## Implementation Summary
Implemented WebSocket protocol v2 across Go and Qt: stable node identities, field operations, immediate peer updates, 500ms/100-payload/128KiB grouped durable commits, account-scoped exact-hash receipts, bounded queues and restart-safe retries. CouchDB small receipts use checked bulk writes; full-map batches use the head CAS as commit point. Independent fields merge; same-field conflicts follow server arrival order.

Qt preserves pending edits, selection, file dirty state and rebased undo/redo. Legacy offline snapshots are retained and exported as recoverable .omm files. Storage-failure tests protect unqueued edits from incoming replacement, and a retained handshake allows synchronization to resume on the next live update after storage recovers. Media validation prevents invalid shared snapshots from poisoning clients.

Validation: Go full race suite and vet passed; real CouchDB v1 and v2 WebSocket tests passed against localhost:5984 using disposable databases, without Docker or touching user maps. Qt fast checks passed 12/12 and the macOS app build passed. Backend binary built at /tmp/mindarchy-server-live-v2. App launched from build-macos before the final recovery adjustment. Normal restart after the final rebuild was cancelled by the app (-128); the running session was preserved. The latest binary is ready for next launch. No backend deployment, packaging or signing performed.

Known scope: single Go process; full canonical state broadcasts; retained small receipt documents; no character-level CRDT. Production throughput not yet measured.
