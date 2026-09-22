# Collaboration storage and live editing

## Implemented changes

- Shared-map listing uses the `mindarchy-membership-v1/by_account` view. It returns names, IDs, owner and the requesting account's role, without snapshots or other members' identities. The index handles legacy base64-encoded heads and new heads with explicit metadata. The first query builds the view; subsequent queries update it incrementally.
- `GET /v1/maps?limit=100&after=<last-id>` returns an array plus `X-Next-Cursor` when another page exists. Maximum page size is 200. Omit `limit` for compatibility with existing Qt clients: the server gathers indexed pages and returns the complete summary array. Thus the old client still uses memory proportional to its own map count; it no longer scans everyone's maps/history. Cursor pages are not a transactional snapshot across concurrent membership changes.
- Loading a current map fetches its head and one latest-state record. Permissions check only the head. Broken latest state fails explicitly rather than silently returning the initial snapshot.
- Committed submission receipts have deterministic IDs based on map, account, device and counter. Legacy history is indexed once on first submission/maintenance. The last committed batch acts as a durable receipt until the next submission indexes it before advancing the head. Receipt indexing errors block advancement; an uncommitted orphan never becomes a receipt. The old unbounded in-memory receipt cache is removed.
- Explicit checkpoints retain a configurable recent batch window (default 100). They preserve durable receipts, create a current replay boundary and schedule old snapshots/batches for collection after a grace period (default 24h, minimum 1h). Collection resumes after interruptions. Current state and retries from offline clients remain available. Snapshot checkpoint IDs include sequence to avoid reusing CouchDB tombstones.
- Qt skips consecutive identical serialized map payloads. A remote update refreshes the comparison baseline, so editing back to an earlier local value still syncs. Failure to write the local outbox prevents sending the edit as if it were safely queued.
- Duplicate retries are acknowledged only to their sender; historical payloads are never rebroadcast as new edits.
- WebSocket authorization is checked after receiving a frame, fixing an existing race where a session could expire while the server blocked waiting for an edit.

## Deployment

These storage changes require rebuilding and restarting the **Go server**, not only rebuilding the desktop app. Build from `qt-prototype/backend`:

```sh
go build -o mindarchy-server ./cmd/mindarchy-server
go build -o mindarchy-maintenance ./cmd/mindarchy-maintenance
```

Use the existing `MINDARCHY_COUCHDB_URL`, `MINDARCHY_COUCHDB_DATABASE`, `MINDARCHY_COUCHDB_USER` and `MINDARCHY_COUCHDB_PASSWORD` environment configuration. Startup installs the versioned design document; that account must be allowed to create design documents. Initial view indexing can take time on a large existing database. No deployment was performed by this change.

Run a single active Go writer per database for now. Stop old server versions before upgrading; do not mix versions or downgrade after collection. Existing older servers replay pruned history and do not preserve the new metadata. Back up the database before first enabling collection.

Maintenance is explicit and per map. Run checkpoint/collection while the Go server is stopped so active in-memory rooms cannot race head maintenance. Without `-apply`, these commands only inspect the head:

```sh
./mindarchy-maintenance -map MAP_ID
./mindarchy-maintenance -map MAP_ID -action checkpoint -keep 100 -grace 24h -apply
# After the grace period:
./mindarchy-maintenance -map MAP_ID -action collect -apply
```

New checkpoints require collecting any pending plan first. Checkpoints do not run on each save. An operator can schedule them; no scheduler was installed. Small receipt records and GC manifests are retained, so total document count is not bounded: this primarily bounds expensive full-map history. CouchDB compaction reclaims deleted bodies later; deletion tombstones remain. Failed writes before head CAS may leave orphan immutable records, which this conservative collector deliberately does not sweep.

## Live WebSocket protocol v2

Implemented path: `Qt durable outbox → WebSocket → Go room merge → immediate applied event → grouped CouchDB commit → durable event`.

Qt sends stable-ID node/field operations. Independent fields merge; edits to the same field follow server arrival order. Deletion wins over later edits to missing nodes. This is not character-level text CRDT merging. Incoming state preserves local pending changes, selection, file dirty state and rebased undo/redo. Older pending full snapshots remain intact and export as recoverable `.omm` files with an explicit recovery message.

Go broadcasts accepted edits immediately, then flushes at 500 ms since the first pending edit, 100 edit payloads or 128 KiB of operation bytes. Pending limits are 1,000 payloads / 4 MiB; documents are limited to 1 MiB. Presence remains ephemeral. These are initial operational limits, not measured capacity claims. CouchDB I/O during a flush serializes room processing, so storage latency can still affect edit latency.

Only a successful head CAS produces a durable acknowledgement. Qt removes outbox entries only for matching account, operation ID and payload hash. Retries survive process restart and ambiguous commit responses. Storage failures retain pending edits and retry; UI distinguishes live visibility from durable save. Authorisation is checked when accepting each edit: revocation blocks future edits, while already accepted edits may finish committing.

Each flush writes one full-map batch and advances its head. Small per-operation receipts remain for retry lookup; receipt materialization uses `_bulk_docs` and checks every result, including conflicts and partial failures. This reduces full-map writes and HTTP requests, but does not eliminate per-operation receipt documents. The head remains the commit point because bulk writes are not atomic.

The Qt client negotiates `?protocol=2` and falls back when an older server responds with v1 hello. A map is promoted on its first v2 connection; subsequent v1 writes are rejected to prevent whole-map snapshots overwriting merged edits. Upgrade all collaborating clients and the server together. Run only one active Go server per database; multiple servers require room ownership, fencing and cross-server routing.

This version broadcasts canonical full state with live events for deterministic reconciliation. Client-to-server edits are operations, but server-to-client bandwidth still scales with document size and participant count. Delta broadcasts and multi-server room routing are later optimization work; load-test before claiming large-room capacity.

Wire details: [Live v2 contract](superpowers/specs/2026-09-22-live-v2-contract.md).

## Verification and measurements

Repeat locally:

```sh
cd backend
go test -race ./...
go test ./integration -run '^$' -bench BenchmarkCurrentMapRead -benchtime=200ms
```

Synthetic in-memory current-map reads, Apple M1 Max, 2026-09-22:

| Prior updates | Immutable records fetched per open | CPU time per open |
| ---: | ---: | ---: |
| 1 | 1 | 2.26 µs |
| 1,000 | 1 | 2.57 µs |
| 10,000 | 1 | 2.71 µs |

These are algorithmic measurements, **not** CouchDB/network throughput estimates. The pre-change regression fetched 1,001 immutable records at 1,000 updates. New-submission lookup after restart is also bounded in the synthetic regression rather than scanning prior batches.

Covered: legacy receipt migration, failed head commit/orphans, receipt storage failure, crash/restart retry, counter reuse, missing latest snapshot, checkpoint grace, old retry after collection, repeated historical snapshot contents with tombstones, Unicode legacy view payloads, account-scoped pagination, WebSocket session expiry while idle and Qt identical-payload suppression including remote-update interaction.

Real CouchDB integration uses `scripts/test-collaboration-couchdb.sh` with a dedicated disposable test database. The v1 restart and v2 multi-editor WebSocket integration tests passed against the existing bare-metal CouchDB on localhost:5984 using disposable test databases; Docker was not used. Production latency/write-rate measurements remain outstanding. Before deployment, run this gate and measure map listing, reconnect, p95 submit latency, writes per second and disk growth under concurrent editing. Partition migration remains a later evidence-based decision.
