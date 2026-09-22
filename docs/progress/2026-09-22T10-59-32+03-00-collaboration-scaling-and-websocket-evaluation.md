# Collaboration scaling and WebSocket evaluation

## Initial Prompt
Proceed with the proposed plan in order and evaluate live editing through Go WebSockets to minimize CouchDB writes.

## Plan
Implement bounded reads/indexed listings/receipts first; add explicit safe retention; suppress unchanged Qt submissions; evaluate the v2 live protocol; measure and verify.

## Proposed Next Steps
Run real CouchDB integration and load tests on a disposable database before deployment. Upgrade the Go server as well as Qt. Implement versioned node operations with merge semantics before enabling immediate live relay and grouped durability; keep the current acknowledgement guarantee meanwhile.

## Implementation Summary
Implemented indexed account-scoped summary listing with optional cursor pagination and legacy-head support; direct latest-state reads and head-only authorization; durable deterministic receipt lookup with legacy migration and crash-safe materialization. Added explicit checkpoint/collection maintenance with grace period, retained receipts and sequence-specific snapshot IDs. Added Qt identical-payload suppression with remote-state baseline refresh and local-outbox failure handling. Fixed WebSocket authentication-before-blocking-read race and historical retry rebroadcast rollback. All Go race suites pass; macOS app build and all 11 fast Qt suites pass. Synthetic current-state reads remain one immutable record at 1/1,000/10,000 historical updates (2.26/2.57/2.71 microseconds CPU-only). Independent review findings fixed and regression-tested. Real CouchDB integration/load testing not run because Docker daemon is stopped; live data and deployment unchanged. Immediate WebSocket relay with grouped durable writes is evaluated/documented, not enabled: it requires versioned mergeable operations and distinct durable acknowledgements. Qt app normal quit returned User cancelled; existing session preserved, rebuilt app ready for next restart. Changes remain uncommitted.
