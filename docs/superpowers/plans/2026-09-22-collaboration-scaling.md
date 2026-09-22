# Collaboration Scaling Implementation Plan

Goal: execute the four approved stages in order, first making reads independent of edit-history length.
Spec: ../specs/2026-09-22-collaboration-scaling-design.md
Architecture: retain Go as authorization and WebSocket coordinator; CouchDB remains authoritative for durable acknowledgements. Keep legacy full-snapshot wire compatibility during stage 1.

- [x] 1a. Add tests for direct current-state reads and head-only authorization, including corrupt latest state; change sharing/service.go to use the committed batch directly.
- [x] 1b. Test indexed membership queries and pagination in couch; add a versioned view with legacy head decoding, startup initialization, summary-only service listing and optional HTTP cursor pagination. Preserve legacy array consumers.
- [x] 1c. Test restart, duplicate retry, counter reuse, orphan CAS failure and legacy migration; implement durable deterministic receipt indexing and head marker, preserving marker on ACL updates.
- [x] Verify stage 1 with all Go tests and race detector; document query-count evidence and deployment instructions.
- [x] 2. Implement checkpoint/retention only after stage 1 recovery tests pass, with explicit maintenance invocation and grace period.
- [x] 3. Evaluate WebSocket preview versus durable-ack protocol and node-operation migration; implement unchanged-payload suppression separately from protocol v2.
- [x] 4a. Add repeatable synthetic history-length measurements.
- [ ] 4b. Run real CouchDB integration and production-like load measurements (local Docker daemon unavailable).
- [ ] Next protocol phase: wire node-level operations/CRDT merge into Qt and implement v2 immediate relay with grouped durable acknowledgements. Evaluation is complete; this protocol is not implemented by the current change.

Review focus: legacy documents, storage failure between receipt and head writes, permissions changed by another process, malformed history, and old clients retrying submissions after a server restart.
