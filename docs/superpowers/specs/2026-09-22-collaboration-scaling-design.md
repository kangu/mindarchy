# Collaboration scaling

User intent: execute the agreed four stages sequentially, and evaluate live WebSocket editing through Go with fewer CouchDB writes. Preserve existing maps, permissions, retry safety and offline recovery. No live database changes during development.

## Stage 1: bounded reads
The current wire payload is a complete JSON snapshot, not an Automerge delta. Read the head's last committed batch directly; never reconstruct a full-snapshot map through its history. Authorization reads only the head. List account membership with a CouchDB view, returning summaries without snapshots and supporting cursor pagination; legacy clients may request all summaries, new callers can request bounded pages. The view indexes legacy base64 head payloads as well as new top-level head metadata, avoiding a destructive migration. Install the versioned design document at startup.

Duplicate detection uses deterministic immutable receipt records indexed by map/account/device/counter. A head marks receipt indexing complete. Before advancing a head, persist its latest batch receipt; this closes the crash window between commit and receipt materialization. Legacy chains are indexed once before the first new commit, and the marker is advanced atomically with the head. Never index unreachable/orphan batches. Missing records mean new submissions only after indexing. Preserve the original receipt and reject counter reuse with different bytes.

## Stage 2: bounded history
Checkpoint full-snapshot streams and retain a configurable recent window. Before deleting batches, all committed receipts must be durable and old clients must be able to resync without losing pending local edits. Use an explicit maintenance path and grace period; do not silently delete existing history in stage 1. CouchDB compaction is separate from application retention.

## Stage 3: live relay and incremental edits
Existing Go WebSocket rooms already relay commits after durable writes. Do not label memory-only broadcasts committed. The proposed v2 protocol has ephemeral preview/operation events and separate durable acknowledgements. Clients retain a durable outbox until acknowledgement. A room coalesces durable writes with a short maximum flush latency and byte threshold, flushes on orderly shutdown, and applies backpressure on storage failure. A crash recovers through client retries. Whole-map previews must not overwrite concurrent edits; true collaborative editing requires node operations/CRDT state with stable IDs, dependency handling and deterministic merges first. Presence and cursor events never require CouchDB writes. Multi-server operation requires single room ownership/fencing before enabling multiple writers.

## Stage 4: measurement
Measure request counts and latency at growing history lengths, account isolation, reconnection, restart and crash boundaries. Use synthetic data. Partition migration is a separate measured decision; current map: prefixes cannot directly become per-map partition keys.

## Safety cases
Stale ACLs fail closed; listing errors are surfaced. Corrupt latest batches fail explicitly rather than returning old state. Receipt indexing failure prevents head advancement. A failed CAS cannot make an orphan batch look committed. Existing arrays returned by /v1/maps remain compatible. No tests send real invitations or manipulate user maps.
