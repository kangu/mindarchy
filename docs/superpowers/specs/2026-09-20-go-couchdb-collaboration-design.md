# Go-coordinated live mindmap sharing

Status: proposed design accompanying the requested implementation plan; no application or service implementation is authorized by this document alone.

## Intent and scope

Users on different accounts can edit the same mindmap simultaneously and see who is present. The backend is a thin Go service in front of central CouchDB, doing authentication integration, authorization, room coordination, edit validation, batching, persistence and recovery. CouchDB is the durable database, not the live editing engine.

Confirmed scope includes full offline shared editing with automatic reconciliation. Initial deployment assumptions: one active Go process, one logical CouchDB write authority, authenticated invitations to existing accounts and owner/editor/viewer roles. Existing local maps continue to work without accounts. No public links, anonymous collaboration, email delivery, horizontal active-active rooms or end-to-end encryption in this release.

The desktop remains Qt 6.6+ / C++20 on Linux, macOS and Windows. Keep portable `.omm` v1 import/export. The Go service does not render maps, run Qt, or execute per-client layout. Do not introduce Redis, Kafka, a Node service or a custom CRDT implementation.

## Responsibility boundaries

```text
Qt desktop: editing, incremental model projection, local pending queue
    HTTPS / WebSocket
Go service: identity checks, map rooms, validation, presence, durable commits
    authenticated private HTTP
CouchDB: heads/permissions, immutable update batches, snapshots, assets
```

Use Go `net/http`, `github.com/coder/websocket`, and a narrow CouchDB HTTP client using pooled connections. Use an external OIDC identity provider with native authorization-code/PKCE login; Go validates issuer, audience, signature and expiry. Account identity is `(issuer, subject)`, never an email or a caller-supplied user ID. Store credentials in platform credential stores. Deployment pins a security-supported Go toolchain and exact dependency/database versions after the integration gate; the document makes no claim that untested bindings or current versions already work.

Use Automerge as the initial collaboration-engine candidate: `automerge-go` in the server and matching Automerge C bindings behind a C++ adapter. The upstream Go binding uses cgo; this is a native build dependency, not a second service. Before committing to it, prove binary interoperability, rich-text marks, undo feasibility, supported desktop builds and bounded memory. If those fail, report the concrete failure and revise the engine choice; do not silently replace concurrent text editing with whole-string last-writer-wins or build an original merge algorithm.

## Collaborative document

Each map and node has a UUID. Initially retain runtime integer IDs in the existing engine through a per-document UUID-to-integer table, avoiding unrelated canvas refactoring. UUIDs are never reused. The collaboration document holds nodes with parent registers, ordered child-reference CRDT lists, monotonic deletion markers, relationships, map settings and asset references. Effective children and parents are derived together by the projection rules below, never trusted as independently supplied JSON. Preserve Task, Date, Meeting and calendar data. Convert supported rich title formatting to collaborative text/marks; notes and date-entry text are shared text too. Import unsupported formatting must produce a visible preservation/export path, never silent loss.

The Go adapter validates candidate changes before acceptance: known schema, one immutable root, immutable node identity, field types, node/image quotas and valid rich-text attributes. It verifies the projected tree has one parent for every live non-root node and no cycles/disconnected nodes. Materialize only the changed fields/subtrees in normal operation; benchmark whole-document validation as a fallback and optimize based on measured cost. Validate updates on isolated candidate state so rejected changes do not poison the authoritative room.

CRDT convergence does not replace application rules. Concurrent insertions and text edits merge automatically. Scalar fields and parent registers use the engine's deterministic winner, retaining competing values in collaboration state. After merge, apply the following pure projection identically in Go and C++: (1) hide monotonically deleted nodes; (2) derive each live node's parent from its winning register; attach missing/deleted/self-parent references to root; (3) for every remaining parent cycle, attach its lexicographically smallest UUID to root; (4) order children by the parent's merged child-reference list, keeping only the first reference to each child whose effective parent matches, then append unlisted children sorted by UUID; (5) hide relationships whose endpoints are deleted/missing. Projection never emits corrective edits, so peers cannot get into repair loops.

Delete-branch marks the node and the descendants observed by the deleting client; an unseen concurrent child survives and appears under root with a recovery notice. Concurrent text on a deleted node remains in retained collaboration state and can be copied into a new node; deletion wins visibility. A move inserts a reference into the target ordered list and changes the node's parent in one local CRDT transaction. The client removes obsolete observed references without assuming all remote references are known. These rules guarantee convergence and tree validity without pretending every user's structural intention can win.

Malformed/schema-invalid, quota-exceeding or unauthorized changes are rejected. Preserve the rejected local draft, reset the client to accepted state, and stop automatically replaying its causally dependent tail. Offer copy/export and explicit reapplication; ordinary concurrent moves/deletes are reconciled by projection, not rejected. Test actual Automerge conflict visibility and freeze its adapter projection in shared protocol fixtures.

Undo is actor-scoped semantic compensation, never restoration of a whole-map snapshot. Delete only the actor's inserted text, restore deleted text using fresh identities, and condition scalar/structural reversals on the current value still matching the operation being undone. Return `undo_conflict` when another participant's work would be overwritten; preserve a recoverable draft. Undo does not bypass current authorization. The first integration gate must demonstrate the required text identity/mark APIs.

Selection, viewport, hover and folding remain local session state. Shared manual positions and layout/theme choices are persistent content; coalesce drag updates and send a final position. Presence advertises authenticated display identity, selected node UUID and optional cursor anchor, not raw Qt integer IDs. Replacing full JSON via `loadDocumentBytes()` on each update is prohibited: it resets selection and affects undo.

## Permissions and APIs

Owner can invite existing accounts, change roles and delete a map; editor can change content; viewer can read content and join presence. Owner transfer is excluded initially. No user is a CouchDB administrator or database member; only the Go service can reach application databases. Provision restricted `_security` explicitly.

HTTP endpoints: `GET /v1/me`, `POST /v1/maps`, `GET /v1/maps`, `GET /v1/maps/{id}`, `POST /v1/maps/{id}/invites`, `POST /v1/invites/{token}/accept`, `PATCH /v1/maps/{id}/members/{account}`, `DELETE /v1/maps/{id}/members/{account}`, `DELETE /v1/maps/{id}`, `PUT /v1/maps/{id}/assets/{sha256}`, `GET /v1/maps/{id}/assets/{sha256}` and `GET /v1/maps/{id}/live` (WebSocket upgrade). Invitation tokens are random, stored hashed, expire after seven days and are bound to the intended account; acceptance is idempotent. Invite links are copied manually; no email integration is needed.

All map-specific reads, writes, joins and permission changes go through the map coordinator. List/discovery indexes are hints only: verify current head ACL before exposing a map. Revocation is an ordered, durable head transition that removes the session from fanout and rejects subsequent submissions. Broadcast only to currently authorized sessions, including snapshot/reconnect responses. Token expiry closes the socket unless reauthentication succeeds. Unknown and inaccessible resources both return 404 to unauthorized callers. A recipient removed while offline may retain downloaded content, but cannot submit or fetch more.

## Room execution and transport

One coordinator goroutine per active map owns its committed CRDT state, head, client counters and presence table. Only the coordinator mutates room state. One WebSocket reader and one writer per connection; all network/storage work uses deadlines and bounded queues. CouchDB persistence uses a bounded worker pool, while each room permits only one in-flight commit. Pending edits received during a commit are queued; control messages and heartbeat processing stay responsive. Order permission changes with edits, never execute them around an in-flight commit.

WebSocket protocol v1: `hello`, `snapshot`, `submit`, `committed`, `presence`, `rejected`, `resync_required`, `access_revoked`. Use JSON control envelopes; base64 update payloads initially, with explicit decoded-byte limits. Benchmark binary framing before adding it. Submit carries protocol/map IDs, random device ID, monotonically increasing device counter, update hash and CRDT change bytes. The authenticated account is assigned by the server. Only one submission per device is in flight; later local changes are queued durably. Reusing a counter with a different hash is rejected.

`committed` means the update is in CouchDB's committed head chain. Clients render their own pending edits immediately and persist them locally, showing `Saved locally / Waiting to sync` until acknowledged. Peers receive only committed edits in the first release. Disconnected editors continue working against cached collaboration state and permissions. Reconnect checks current authorization, obtains missing remote changes and exchanges local changes using the engine's causal sync protocol. Pending submissions are idempotently committed and projection reconciles ordinary concurrent structure/text automatically. A locally cached editor permission is not authority to write after reconnect: revocation or downgrade blocks upload and preserves the offline work as a private recoverable draft. Missing dependencies request sync instead of dropping an update.

Initial configurable bounds: 25 ms batch window, 64 updates or 256 KiB decoded data per normal batch, 256 queued edit submissions per room, 64 outgoing edit messages per connection, 1 MiB incoming decoded update maximum and 32 participants per room. A single valid update between 256 KiB and 1 MiB is committed alone. Before generating changes, clients split large paste/import actions into bounded semantic transactions with one local undo group; never split opaque CRDT bytes arbitrarily. An indivisible update over 1 MiB returns an explicit size error and retains the local draft. Initial map creation via `POST /v1/maps` accepts a validated snapshot under the existing document/image budgets, using a separate bounded import body limit. Latest presence replaces older presence, at most 10 Hz per client; heartbeat every 15 seconds, expire after 45 seconds. Slow clients receive `resync_required` and disconnect instead of blocking a room. Drop presence before content. Bound total active-room memory; evict only idle committed rooms after five minutes, and reject new rooms when the process budget is exhausted.

## CouchDB layout and commit protocol

Use a private content database and a private identity/invitation database. In the content database:

- `map:<uuid>:head`: current head revision, monotonic sequence, latest batch ID, snapshot pointer/covered sequence, schema version, authoritative ACL, accepted invitation hashes and deletion flag. Invitation acceptance records its hash in the same CAS as the grant, so a later revoked grant cannot be recreated by replaying an old invitation after an ambiguous response.
- `map:<uuid>:batch:<hash>`: immutable parent-batch pointer, sequence range, authenticated submitters, per-device counters/hashes and update bytes. Hash covers the canonical complete batch, including its parent; same map retry is idempotent.
- `map:<uuid>:snapshot:<hash>`: immutable collaboration state, covered sequence/batch, schema and deduplication counters/hashes.
- `map:<uuid>:asset:<sha256>`: immutable validated binary attachment and media metadata. Asset access always checks the map ACL.

Durable commit order: validate/fork candidate -> write immutable batch -> CAS the current head using `_rev` -> install candidate and fan out `committed`. A batch not reachable from the committed head is not accepted. ACL changes and deletion also CAS the same head and advance the ordered sequence. No cross-document transaction is assumed. A failed or ambiguous write is resolved by reading the head and verifying chain membership before acknowledging or retrying. On a competing head revision, freeze the room, reload and reauthorize; never blindly retry an ACL/content write. Reject unexpected CouchDB leaf conflicts and require operational recovery rather than choosing a hidden winner.

At startup/rejoin, load snapshot then replay only batches reachable from the head. Counters and hashes in snapshots plus post-snapshot replay suppress duplicate application across restart. Content and permission changes share the head sequence; batch sequence ranges may have gaps caused by metadata transitions. Do not infer missing content from those gaps. An orphan immutable document must never grant access or become visible as an accepted edit.

Checkpoint after 500 accepted updates or 5 MiB of update data, whichever comes first. Snapshot a stable committed state on a bounded background worker; publish its pointer through the room only after verifying the stored bytes/hash and covered chain point. Updates newer than the snapshot remain replayable. Snapshots must retain CRDT causal history/tombstones required by long-disconnected clients, not just materialized JSON. Initially keep committed update history; postpone destructive history pruning until a separate retention/restore policy is approved. Compact CouchDB on an operational schedule, but do not use CouchDB `_rev` bodies as application history. Orphan assets/batches can be reclaimed only after a 24-hour grace period, a reachability check including retained history and confirmation that no upload/commit references them.

Deploy exactly one active coordinator process (replacement strategy, not rolling overlap) for v1. CouchDB CAS is a stale-writer detector, not a distributed room lease. Horizontal scaling requires separately designed room ownership with fencing and routing; do not enable it with just a load balancer. This is a deliberate first-release operational limit.

## Assets and local persistence

Upload assets separately before referencing them in an accepted edit. Verify hash, MIME, byte size and decoded pixel bounds; do not allow global asset existence probing. Do not resolve arbitrary external URLs on the server. Local resource links stay explicitly device-local unless the user uploads the file. Keep existing image/document limits; measure serialized/base64 overhead and configure HTTP/CouchDB limits consistently before integration tests.

Desktop collaboration storage is separate from portable `.omm`: identity key, map UUID, accepted collaboration state, full local CRDT state, last sequence, next device counter, pending submissions/assets and rejected draft. Use SQLite transactions and account-separated directories. Persist each offline edit before reporting it locally saved; write pending updates before sending, and commit accepted state/counter before removing pending entries. Use local CRDT transactions as upload units and bounded dependency-ordered batches, not a single unbounded reconnect upload. Do not share a device actor identity between concurrent desktop processes; use a process/session-specific actor with a persisted recovery record. Account switch cancels sockets, purges credentials from memory and detaches caches before another account opens maps.

## Performance acceptance targets

These are proposed test targets, not measured capacity promises. Record hardware, versions, RTT and dataset with every result. On a 4-vCPU/8-GiB Go host with a separate SSD CouchDB host at <=5 ms RTT, exercise 100 active rooms, 10 participants each, 1,000 nodes per map and 1,000 accepted small edits/second total for 15 minutes. Target p95 submit-to-durable-ack <=150 ms, p99 <=300 ms, p95 peer display <=200 ms on a low-latency LAN, no lost acknowledged edits, no unbounded queues and Go RSS <=2 GiB. Include fanout/bytes in results; passing a no-listener test is insufficient.

Separately test a 10,000-node map with 20 editors, 100 small edits/second, reconnect storms, large text and images. Target p95 local input handling <16 ms and no full-map serialization per keystroke. If targets fail, profile CPU, allocations, cgo calls, validation, layout and CouchDB latency separately before changing the architecture. Permission revocation must stop server-authorized delivery immediately after its durable transition; presence expiry must not depend on a healthy database.

## Source checks

- Existing serializer/loader and snapshot undo: `src/engine.cpp`; rich-title editor and explicit notes application: `qml/DocumentWorkspace.qml`; text document access: `src/canvas.cpp`.
- [CouchDB security](https://docs.couchdb.org/en/stable/api/database/security.html): read authorization is database-wide.
- [CouchDB bulk API](https://docs.couchdb.org/en/stable/api/database/bulk-api.html): bulk writes are not a multi-document transaction.
- [CouchDB conflicts](https://docs.couchdb.org/en/stable/replication/conflicts.html): revisions and semantic merge are different concerns.
- [Automerge Go](https://github.com/automerge/automerge-go): cgo wrapper; platform/feature compatibility remains an implementation gate.
- [Coder WebSocket](https://github.com/coder/websocket): candidate Go transport dependency.
- [Qt text-document changes](https://doc.qt.io/qt-6/qtextdocument.html): incremental text/format hooks; Unicode and IME mapping require tests.
