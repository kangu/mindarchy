# CouchDB scaling evaluation

## Initial Prompt
Evaluate growing map documents, performance, and database-per-user versus database-per-map.

## Plan
Trace client submissions and server writes/reads; check current CouchDB documentation; compare architecture options.

## Proposed Next Steps
Measure representative workloads, implement indexed listings and receipt lookup, then checkpoint/retention with restart and offline replay tests. Evaluate partition migration separately.

## Implementation Summary
Reviewed Qt sync and Go CouchDB persistence without modifying application code or live data. Each debounced shared-map edit submits full document bytes; the server appends an immutable batch and updates a stable head. New submissions scan the batch chain for duplicate receipts; map hydration replays history and list requests scan every map-prefixed document. No history pruning/checkpoint implementation was found. Recommend indexed account map listings, direct receipt lookups, bounded history with offline-safe checkpoints, and incremental sync before database proliferation. Keep shared storage behind the Go authorization layer; consider map partitions after measurement, with migrated IDs because current map: prefix would put every document in one partition. No runtime performance benchmark or live database inspection performed.
