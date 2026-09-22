# Go and CouchDB Live Collaboration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Execution mode has not been selected; this document does not initiate implementation.

**Goal:** Let different accounts share mindmaps, edit simultaneously with presence, and continue editing offline with automatic reconciliation, using a thin Go coordination service and central CouchDB.

**Architecture:** One Go process owns active map rooms, authentication integration, authorization, merge validation, presence, batching and durable commits. Clients and Go use a compatible collaboration engine; CouchDB stores immutable updates, collaboration snapshots, assets and revision-controlled map heads. The first release has one active coordinator, with bounded concurrency across rooms and no external message broker.

**Tech Stack:** Go `net/http`, Coder WebSocket, CouchDB HTTP API, OIDC/PKCE, Qt 6.6+ / C++20, Qt Network/WebSockets/SQL and SQLite. Automerge Go/C bindings are the proposed collaboration dependency, subject to the explicit compatibility gate in Task 2.

**Spec:** [Go-coordinated live mindmap sharing](../specs/2026-09-20-go-couchdb-collaboration-design.md). Read it before this plan. It records confirmed requirements, initial deployment assumptions, exact merge rules and proposed performance targets. The assessment in `docs/progress/2026-09-20T12-58-34+0300-couchdb-sharing-evaluation.md` is background; this spec supersedes its snapshot-only and online-only alternatives.

## Global Constraints

- Confirmed scope includes full offline shared editing with automatic reconciliation.
- The desktop remains Qt 6.6+ / C++20 on Linux, macOS and Windows.
- Keep portable `.omm` v1 import/export.
- Do not introduce Redis, Kafka, a Node service or a custom CRDT implementation.
- Deploy exactly one active coordinator process (replacement strategy, not rolling overlap) for v1.
- No user is a CouchDB administrator or database member; only the Go service can reach application databases.
- `committed` means the update is in CouchDB's committed head chain.
- Undo is actor-scoped semantic compensation, never restoration of a whole-map snapshot.
- Full offline editing includes local assets, durable local saves, automatic causal reconciliation and preservation of work rejected after revocation.
- All timing, capacity and memory numbers below are acceptance targets to measure, not existing capabilities.

## Review Focus

1. A CouchDB timeout after a successful write must not duplicate an edit or lose its acknowledgment: Task 5 fault tests.
2. Revocation during reconnect or an in-flight commit must stop future delivery/upload while retaining the user's local work: Tasks 6 and 9.
3. Unicode/IME and formatting changes must preserve cursor positions and content across C++/Go: Tasks 2 and 10.
4. Concurrent moves/deletion must converge to an acyclic map with a visible route to recovered content: Tasks 3 and 10.
5. Restart/account-switch/multiple desktop processes must not reuse conflicting actor identities or expose another account's cached maps: Task 9.

## Delivery sequence and file responsibilities

Milestone A (Tasks 1–3): a verified collaboration model and protocol, before account UI or production infrastructure work.

Milestone B (Tasks 4–8): a headless Go service with secure sharing, durable room coordination, presence and assets, testable with two synthetic clients.

Milestone C (Tasks 9–11): native desktop integration, offline reconciliation and sharing UX.

Milestone D (Tasks 12–13): load/failure testing, packaging and release readiness.

Keep these as one feature delivered in reviewable increments; each task below owns its tests and commit. Do not add a dependency merely because it appears in a transitive example. Pin exact versions/checksums after Task 2 and record them in `backend/go.mod`, `backend/go.sum` and `third_party/collaboration/versions.json`.

| Path | Responsibility |
|---|---|
| `backend/cmd/mindarchy-server/main.go` | Configuration, dependency construction, graceful shutdown |
| `backend/internal/protocol/` | Versioned envelopes, IDs, limits, errors; no network |
| `backend/internal/collab/` | Native merge adapter, schema validation, document projection |
| `backend/internal/couch/` | HTTP transport, immutable writes, head CAS, replay |
| `backend/internal/rooms/` | Map coordinator, ordered commands, batching, presence |
| `backend/internal/auth/` | OIDC token verification and stable account identities |
| `backend/internal/sharing/` | Map listing, invitation and membership operations |
| `backend/internal/httpapi/` | HTTPS handlers, WebSocket framing, error mapping |
| `backend/internal/assets/` | Validated map-scoped attachments |
| `backend/integration/`, `backend/cmd/collab-load/` | Real-database failure tests and load driver |
| `src/collaboration/` | Native adapter, account/session transport, local storage, engine bridge, undo |
| `tests/collaboration_*_test.cpp` | Native compatibility, storage, session, editing and UI tests |
| `tests/fixtures/collaboration/` | Shared JSON/binary vectors consumed by Go and C++ |
| `qml/ShareDialog.qml`, `qml/SharedMaps.qml`, `qml/PresenceStrip.qml` | User-facing sharing/presence |
| `packaging/server/`, `scripts/test-collaboration.sh` | Private service deployment and reproducible verification |

Existing integration points: `src/engine.h` (`MapNode`, `Engine::State`); `src/engine.cpp` (`checkpoint`, `setText`, `setNotes`, `documentBytes`, `loadDocumentBytes`); `src/canvas.cpp` (inline editor and `QTextDocument` access); `qml/DocumentWorkspace.qml` (title/notes editors); `src/main.cpp` and `src/macapplication.cpp` (workspace/session ownership). Update both `CMakeLists.txt` and `prototype.pro`, plus affected test `.pro` files and platform packaging scripts when adding native libraries or Qt modules.

## Shared contracts

Task 1 defines the wire types and Task 2 defines the merge-adapter interface. Names below are proposed project-owned APIs, not claims about upstream APIs.

```go
// backend/internal/protocol/types.go
type AccountID string // server-generated mapping of verified issuer + subject
type MapID string
type DeviceID string
type Head struct {
    Rev string
    Seq uint64
    BatchID, SnapshotID string
    SnapshotSeq uint64
    ACL map[AccountID]string // owner | editor | viewer
    Deleted bool
}
type Submit struct {
    Version int `json:"version"`
    MapID MapID `json:"mapId"`
    DeviceID DeviceID `json:"deviceId"`
    Counter uint64 `json:"counter"`
    Hash string `json:"hash"`
    Changes []byte `json:"changes"`
}
type Receipt struct {
    DeviceID DeviceID `json:"deviceId"`
    Counter uint64 `json:"counter"`
    Hash string `json:"hash"`
    Seq uint64 `json:"seq"`
}
// Server-authenticated account is passed separately, never decoded from Submit.
```

`Changes` carries durable CRDT changes, not a transient sync-session message. Handshake state vectors/heads and dependency requests are separate frames; reconnect must not mistake receiving a sync message for accepting or persisting an edit. Hash is SHA-256 of the exact change payload; size validation occurs before decoding native data. Device counters are scoped by account, map and device. Explicit JSON tags and round-trip fixtures cover every wire/storage type; storage schema is independent of Go exported field spelling.

```go
// backend/internal/collab/document.go
type Document interface {
    Fork() (Document, error)
    Apply(changes []byte) error
    Save() ([]byte, error) // retains causal history and deletion markers
    Heads() []string
    MissingChanges(remoteHeads []string) ([]byte, error)
    Project() (Projection, error)
    Close()
}
// Projection is the validated, renderable node/relationship/settings model
// defined by schema.json and the shared fixtures, not an arbitrary JSON map.
// backend/internal/couch/store.go
type Store interface {
    LoadHead(context.Context, protocol.MapID) (protocol.Head, error)
    GetImmutable(context.Context, string) ([]byte, error)
    PutImmutable(context.Context, string, []byte) error
    CompareAndSwapHead(context.Context, protocol.MapID, string, protocol.Head) (protocol.Head, error)
}
```

The adapter also owns `Load([]byte) (Document, error)`, semantic local editing/undo interfaces and schema validation. Task 2 specifies concrete native ownership/error behavior after verifying upstream APIs. Never expose native handles across UI/network threads; use RAII in C++ and explicit `Close` in Go. Task 3 defines `Projection` with typed UUID nodes and lists, using the common JSON schema.

### Task 1: Freeze protocol, fixtures and limits

**Files:** Create `docs/collaboration-protocol.md`, `tests/fixtures/collaboration/schema.json`, `tests/fixtures/collaboration/wire.json`, `backend/go.mod`, `backend/internal/protocol/{types.go,decode.go,decode_test.go}`.

**Consumes:** Design document. **Produces:** `protocol.Submit`, `Receipt`, `Head`, `DecodeSubmit([]byte) (Submit,error)` and documented error codes.

- [ ] Write table-driven decoding tests with fixtures for a valid message, unknown protocol, duplicate fields, oversized base64, malformed IDs, counter zero, hash mismatch and forged account data. Require `unsupported_version`, `invalid_message`, `too_large`, `counter_reuse`, `missing_dependencies`, `access_revoked`, `resync_required`, `quota_exceeded` and `undo_conflict` to be distinct machine-readable outcomes.

```json
{"version":1,"mapId":"a6e7db7b-81a6-43e2-a1cf-25421f4f81e1","deviceId":"1dbf30b8-fad5-4ef1-a8fa-fc8a7ba8eb65","counter":1,"hash":"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","changes":""}
```

This empty-change fixture is a negative semantic case, not a valid edit. Add real positive payloads from Task 2; keep envelope and CRDT validation tests separate.

- [ ] Run `cd backend && go test ./internal/protocol -run TestDecodeSubmit -v`; demonstrate missing/incorrect decoder behavior before implementation.
- [ ] Implement strict decoding, decoded-byte limits, UUID/hash checks, canonical batch hash encoding and stable error mapping. Document frame direction, dependency sync, receipt semantics, account/device counter scope and authorization requirements for every HTTP endpoint from the spec.
- [ ] Re-run protocol tests; add `FuzzDecodeSubmit` and run `go test ./internal/protocol -fuzz=FuzzDecodeSubmit -fuzztime=30s`.
- [ ] Commit the protocol, fixtures and tests. No production listener is exposed in this task.

### Task 2: Prove native merge compatibility before building around it

**Files:** Create `backend/internal/collab/{document.go,automerge.go,compat_test.go}`, `src/collaboration/{document.h,document.cpp}`, `tests/collaboration_compat_test.cpp`, `third_party/collaboration/versions.json`, `cmake/Collaboration.cmake`; modify `CMakeLists.txt`, `prototype.pro`.

**Consumes:** Task 1 schema and fixtures. **Produces:** The `Document` adapter and `Mindarchy::CollaborationDocument` C++ equivalent, plus pinned native binaries/build recipes.

- [ ] Write interoperability tests: C++ creates shared text, Go applies/edits it, C++ merges both; repeat with two disconnected writers, Unicode surrogate pairs, combining marks, multi-line text, concurrent marks and reversed delivery order. Save/reload between every leg. Assert identical projected JSON and retained causal heads after duplicate delivery.

```json
{"case":"concurrent-insert","initial":"AB","left":{"insertAfter":"A","text":"x"},"right":{"insertAfter":"A","text":"y"},"assert":{"allPeersEqual":true,"containsOnce":["A","B","x","y"]}}
```

- [ ] Run the Go/native compatibility targets to show the adapter is absent; implement only the adapter and fixtures. Verify actual upstream APIs for changes, marks, forks, text identities, conflicts, serialization, error handling and native memory ownership. Do not invent wrapper methods that cannot be implemented.
- [ ] Build the adapter on Linux, macOS and Windows with supported Qt; exercise Go server builds on Linux amd64 and arm64. Record exact upstream commits, compatible native library versions, licenses and compiler flags in `versions.json`.
- [ ] Run `go test ./internal/collab -run TestInterop -v`, `ctest --test-dir build -R collaboration_compat --output-on-failure`, and `go test ./internal/collab -bench . -benchmem`. Measure applying/forking/saving a 10,000-node-equivalent document and cumulative memory after 100,000 small edits.
- [ ] Gate: rich-text round trips, causal sync, actor-scoped text undo and all target builds must work. Record failures and revise the dependency choice before continuing if any fail. No silently reduced text/OS/offline support. Commit the verified adapter and build recipes only after this gate passes.

### Task 3: Define convergent mindmap semantics

**Files:** Create `backend/internal/collab/{schema.go,projection.go,projection_test.go}`, `src/collaboration/{mapprojection.h,mapprojection.cpp}`, `tests/collaboration_model_test.cpp`, `tests/fixtures/collaboration/{moves.json,deletes.json,ordering.json,features.json}`.

**Consumes:** Verified adapter. **Produces:** Typed `Projection`, `Validate(Document) error`, deterministic `Project`, local semantic commands for create/move/delete/text/style/resource/calendar operations.

- [ ] Write shared fixture tests for cycles, deleted parents, simultaneous insertions, duplicated child references, a deleted node receiving text, an unseen child created during branch deletion, root mutation and relationships to deleted nodes. Add all existing node kinds and map settings to round-trip fixtures.

```json
{"case":"cycle","parents":{"a":"b","b":"a"},"root":"root","expectedParents":{"a":"root","b":"a"}}
```

Production IDs are UUIDs; symbolic IDs here describe the fixture. The fixture loader maps them to fixed UUIDs preserving lexical order.

- [ ] Run Go `TestProjectionFixtures` and CTest `collaboration_model` before implementation.
- [ ] Implement the spec's five projection rules and monotonic deletion markers in both adapters; validate schema before projection. Parent-register winner selection must use the same pinned engine semantics on both sides. Reject root mutation and malformed field types. Test permutations of change arrival and confirm the same output without corrective update loops.
- [ ] Implement property/fuzz tests that generate moves/deletes/insertions; assert every visible non-root node reaches root exactly once, all clients converge, and surviving text is preserved. Validate quotas against projected content and native history sizes separately.
- [ ] Run `go test ./internal/collab`, `go test ./internal/collab -fuzz=FuzzProjection -fuzztime=60s` and CTest `collaboration_model`; commit semantics and fixtures.

### Task 4: Build the private CouchDB store and replay path

**Files:** Create `backend/internal/couch/{client.go,store.go,replay.go,store_test.go}`, `backend/integration/couch_test.go`, `packaging/server/compose.test.yaml`.

**Consumes:** Protocol/head schema and document adapter. **Produces:** `Store`, `Replay(ctx,mapID) (Head,Document,error)`.

- [ ] Write integration tests for immutable idempotent writes, same-ID/different-bytes rejection, CAS conflict, unknown write outcome and replay of only the head-reachable batch chain. Include a staged orphan batch and a malformed/missing snapshot.
- [ ] Start a disposable local CouchDB fixture with explicit restrictive `_security`; run `go test ./integration -run TestCouchStore -v` against a unique test database. The harness may delete only its own randomly prefixed test databases.
- [ ] Implement a pooled `http.Client` with request/body/deadline limits, credentials supplied from configuration, status-aware retries and exact conflict/error reporting. Use the regular document API for immutable records and CAS heads; do not treat `_bulk_docs` as an atomic commit. Provision design indexes separately with deployment credentials.
- [ ] Implement replay from the authoritative snapshot/chain, including counter/hash reconstruction and leaf-conflict detection. Reject corrupt hashes, impossible sequence ranges or missing reachable batches with a non-writable room state.
- [ ] Re-run real-CouchDB tests and `go test ./internal/couch`; commit the store. Never require production administrator credentials in desktop configuration.

### Task 5: Implement bounded rooms and durable commit ordering

**Files:** Create `backend/internal/rooms/{manager.go,room.go,commit.go,room_test.go}`, `backend/integration/commit_fault_test.go`.

**Consumes:** `Store`, `Document`, protocol types. **Produces:** `Manager.Submit(ctx,account,submit) (Receipt,error)`, ordered map commands and committed-event subscriptions.

- [ ] Write deterministic coordinator tests with injected store delays/failures. Scenarios: two editors merge, duplicate counter returns original receipt, reused counter/different bytes is rejected, gap requests sync, room quota is bounded and slow persistence does not block heartbeat handling.

```text
submit update U -> immutable batch stored -> kill before head CAS
restart -> U is not committed -> retry U -> exactly one accepted edit

submit update U -> head CAS succeeds -> lose HTTP response -> restart
retry U -> original receipt -> no duplicate edit
```

- [ ] Run `go test ./internal/rooms -run 'TestCommit|TestCounter|TestBounds' -v` and demonstrate failing invariants before implementation.
- [ ] Implement one room coordinator, bounded persistence workers, one in-flight commit/room and 25 ms / 64-update / 256 KiB batching. Validate a fork; stage immutable batch; CAS head; only then swap committed state and fan out. Resume based on exact head membership after ambiguous writes.
- [ ] Process permission commands in the same ordered queue. Freeze/reload a room on unexpected CAS conflict; never acknowledge a speculative state. Evict only idle committed rooms and enforce process-wide memory/admission limits.
- [ ] Run `go test -race ./internal/rooms`, real-CouchDB crash-point tests and room benchmarks with fanout; commit. Race-detector success does not replace native-memory checks from Task 2.

### Task 6: Add accounts, invitations and access control

**Files:** Create `backend/internal/auth/{oidc.go,oidc_test.go}`, `backend/internal/sharing/{service.go,invites.go,service_test.go}`, `backend/internal/httpapi/{maps.go,sharing.go,auth.go}`, `backend/integration/authorization_test.go`.

**Consumes:** Room command queue and CouchDB store. **Produces:** Verified `AccountID`, map creation/listing, invitation acceptance and ordered membership changes.

- [ ] Write tests with an in-process test issuer: wrong issuer/audience, expired token, signature/key rotation, forged subject, viewer submit, editor membership change, revoked reconnect and unauthorized asset/list/snapshot access.
- [ ] Run `go test ./internal/auth ./internal/sharing` and `go test ./integration -run TestAuthorization -v` before adding handlers.
- [ ] Implement OIDC verification with cached bounded JWKS refresh and expiry checks. Map `(issuer,subject)` to a stable account record. Implement all endpoints from the spec using authenticated context, never client-supplied roles/account identity.
- [ ] Implement hashed, account-bound, seven-day invitation tokens and idempotent acceptance. Acceptance updates the head first; marking the invitation consumed can retry. A consumed invitation must never regrant a subsequently revoked membership. Store acceptance identity/transition evidence and validate current invitation/head state through the room.
- [ ] Order revocation with content commits, detach live recipients after the durable transition, and reauthorize every reconnect/missing-change request. Map lists from indexes must recheck ACL. Test revoke racing with receipt delivery, acceptance retry and role downgrade with queued offline work.
- [ ] Run `go test -race ./internal/auth ./internal/sharing ./internal/rooms` and integration authorization tests; commit.

### Task 7: Add WebSockets and transient presence

**Files:** Create `backend/internal/httpapi/{websocket.go,server.go,websocket_test.go}`, `backend/internal/rooms/{presence.go,presence_test.go}`, `backend/cmd/mindarchy-server/main.go`, `backend/integration/live_test.go`.

**Consumes:** Accounts, room subscriptions, durable submit. **Produces:** Protocol v1 live endpoint and presence events.

- [ ] Write two-client tests for snapshot/handshake, dependency exchange, edits, presence, reconnect and token expiry. Add oversized compressed/decompressed input, disallowed browser Origin, slow reader, abandoned socket and CouchDB outage cases.
- [ ] Run `go test ./internal/httpapi ./internal/rooms -run 'TestWebSocket|TestPresence'` before implementation.
- [ ] Implement one reader/one writer per socket, auth header support for native clients, explicit Origin policy for browser requests, finite deadlines and bounded channels. Do not place bearer tokens in URLs or logs. Bind cursor/display identity to authenticated account, and limit arbitrary presence payloads.
- [ ] Coalesce presence at 10 Hz, heartbeat at 15 seconds and expire at 45 seconds. Slow consumers resync instead of stalling others. Deliver committed updates in sequence; permit gaps for ACL-only head transitions. Missing causal dependencies trigger engine sync, not invented sequence filling.
- [ ] Run real two-client integration tests during token expiry, revoke, server shutdown and database outage. Confirm presence causes zero CouchDB writes. Commit live transport and executable wiring.

### Task 8: Add snapshots, assets and controlled recovery

**Files:** Create `backend/internal/rooms/{snapshot.go,snapshot_test.go}`, `backend/internal/assets/{service.go,service_test.go}`, `backend/internal/httpapi/assets.go`, `backend/integration/recovery_test.go`.

**Consumes:** Room commit chain, adapter serialization and permissions. **Produces:** Verified checkpoint publication and map-scoped asset upload/download.

- [ ] Write restart tests with edits committed while an older snapshot is being built, a partially uploaded image, hash mismatch, malicious image dimensions, revoked asset fetch and a client offline across several snapshots.
- [ ] Run `go test ./integration -run 'TestSnapshot|TestAsset|TestOfflineAcrossSnapshots' -v` before implementation.
- [ ] Snapshot stable committed state after 500 updates or 5 MiB; verify bytes and covered chain before publishing the pointer through the room. Retain CRDT history and committed batches. Replay from both old/new snapshot cases must produce identical results.
- [ ] Implement streaming bounded asset uploads and SHA-256 verification, content/image limits consistent with the desktop, and authorization before any body/metadata/existence response. Commit asset metadata only after its attachment is complete. Pending offline assets upload before dependent content; interrupted uploads are idempotently retried.
- [ ] Implement safe orphan inventory/reporting first; deletion needs the spec's grace/reachability/in-flight checks and an explicit configured retention policy. Test backup restore including accounts, heads, snapshots, updates and attachments. Commit.

### Task 9: Implement desktop sessions and full offline persistence

**Files:** Create `src/collaboration/{accountsession.h,accountsession.cpp,localstore.h,localstore.cpp,session.h,session.cpp}`, `tests/collaboration_store_test.cpp`, `tests/collaboration_session_test.cpp`; modify `CMakeLists.txt`, `prototype.pro` and affected test `.pro` files.

**Consumes:** Verified native adapter, protocol, OIDC/live endpoints. **Produces:** `AccountSession`, `LocalStore`, `CollaborationSession` owning local state and sync status.

- [ ] Write Qt tests for offline create/edit/restart/reconnect, duplicate receipts, revoked upload, account switching, two processes editing one cached map and disk-full/local transaction failure. Assert pending data survives restart and never appears in another account's workspace.
- [ ] Add Qt WebSockets and SQL to both build systems; run CTest `collaboration_store|collaboration_session` before implementation.
- [ ] Implement SQLite transactions for full local CRDT state, accepted heads, pending changes/assets, actor/counter records and recoverable rejected drafts. Serialize writes through a per-map process-safe lock; second-process opening activates the existing workspace or creates an independent session actor, never a shared writable actor identity.
- [ ] Implement OIDC authorization-code/PKCE login using system browser and validated callback/state, secure platform token storage and account-scoped cache paths. Keep credentials out of `.omm`, QSettings and logs. Offline cached maps remain usable without a currently valid network token; reconnect requires valid credentials.
- [ ] Reconnect with bounded exponential backoff/jitter, fetch current permissions, exchange causal heads, merge remote changes and submit pending local transactions in dependency order. Distinguish `Saved locally`, `Syncing`, `Synced`, `Access removed` and `Local save failed`; do not label unsaved memory as saved locally.
- [ ] Run native tests with network loss and process termination at each local transaction/receipt boundary. Commit session/store implementation.

### Task 10: Integrate incremental editing, rich text and collaborative undo

**Files:** Create `src/collaboration/{enginebridge.h,enginebridge.cpp,textbridge.h,textbridge.cpp,undomanager.h,undomanager.cpp}`, `tests/collaboration_engine_test.cpp`, `tests/collaboration_text_test.cpp`; modify `src/engine.h`, `src/engine.cpp`, `src/canvas.cpp`, `qml/DocumentWorkspace.qml`, `qml/DateEntryDialog.qml`, `qml/MeetingPanel.qml`.

**Consumes:** Session state, projections and semantic commands. **Produces:** Live editing for existing map features and actor-scoped undo.

- [ ] Write two-engine tests for simultaneous same-title/notes edits, concurrent format changes, move/delete projection, selections/cursors during remote edits, date text and meeting edits, offline images and undo after another user's change. Add Qt IME preedit, emoji, combining characters, multiline paste and rich-title round-trip cases.
- [ ] Run CTest `collaboration_engine|collaboration_text` before integration; keep existing engine/UI tests available as regression coverage.
- [ ] Introduce a UUID/runtime-ID bridge and an incremental remote-apply path that updates only affected nodes and coalesces layout per frame. Maintain local selection/viewport/folding; never call whole-map loader or snapshot restore for a remote keystroke. Local-file editing continues through its existing undo path.
- [ ] Map QTextDocument/QML edits to shared text transactions with explicit UTF-16/native-index conversion and composition handling. Convert the existing notes “Apply” workflow to incremental local CRDT edits for shared maps; preserve local-file behavior. Prevent remote signals from generating echo edits. Preserve current supported rich formatting and reject unsupported transformations visibly.
- [ ] Implement actor-scoped compensation/redo with text identities and current-value preconditions. Show `undo_conflict` rather than overwriting peer changes; undo offline must reconcile after reconnect. Surface automatically reparented content and retained deleted-node drafts through a recovery action.
- [ ] Run new tests plus `ctest --test-dir build -R 'engine|canvas|ui|collaboration' --output-on-failure`. Profile 10,000-node typing; enforce no serialization per keystroke. Commit.

### Task 11: Add sharing and presence UI without changing local-file semantics

**Files:** Create `qml/{ShareDialog.qml,SharedMaps.qml,PresenceStrip.qml}`, `tests/collaboration_ui_test.cpp`; modify `qml/DocumentWorkspace.qml`, `qml/Main.qml`, `src/main.cpp`, `src/macapplication.cpp`, `resources.qrc`, `README.md`, `docs/omm-documents.md`.

**Consumes:** Account/session API and incremental engine integration. **Produces:** Sign-in, Share, role management, Shared with me, sync status and presence UI.

- [ ] Write UI tests: share current local map, accept invite on another account, edit as editor, read-only viewer, revoke online/offline, sign out, private recovery copy and export `.omm`. Verify local maps still open/save with no account/network.
- [ ] Run CTest `collaboration_ui` before creating components.
- [ ] Add Share action and account entry, recipient account selection, owner/editor/viewer controls, copy-invitation-link, Shared with me, presence avatars/node indicators and clear save/sync states. Do not expose CouchDB document IDs, revisions or protocol states in the UI.
- [ ] First sharing creates a new cloud identity from a validated map snapshot and assets; local file remains an independent portable file. Importing/duplicating exported `.omm` creates a fresh cloud identity on sharing. Export uses projected current content, embeds assets and writes no access credentials. Explain unavailable device-local resources at sharing/export time.
- [ ] Test two-account native sessions on all platforms and existing recovery/tab workflows. Avoid adding shortcuts unless necessary; any added shortcut must update `src/macwindow.mm`, `qml/KeyboardShortcuts.qml` and shortcut docs per `agents.md`. Commit UI and documentation.

### Task 12: Measure capacity, convergence and fault recovery

**Files:** Create `backend/cmd/collab-load/main.go`, `backend/integration/chaos_test.go`, `scripts/test-collaboration.sh`, `docs/collaboration-performance.md`; extend native collaboration tests.

**Consumes:** Complete headless and native paths. **Produces:** Repeatable benchmark/failure suite and recorded results.

- [ ] Implement a load driver that creates authenticated test accounts/maps, drives real connected readers/writers, persists receipts and compares final hashes. Include offline edits on both clients followed by reversed/duplicated deliveries. Test fixtures must not target production by default.

```sh
cd backend
go test -race ./...
go test ./internal/collab -bench . -benchmem
go run ./cmd/collab-load --rooms 100 --users-per-room 10 --nodes 1000 --edits-per-second 1000 --duration 15m
```

- [ ] Measure the spec targets: p95 durable ack <=150 ms, p99 <=300 ms, p95 peer display <=200 ms, Go RSS <=2 GiB on documented hardware; report CPU, allocations, native memory, bytes, fanout, CouchDB latency and failed/retried updates. Use a separate native harness for peer-display/input timings; the Go load driver cannot measure Qt rendering.
- [ ] Exercise one 10,000-node/20-editor map at 100 edits/second, a 1,000-client reconnect storm, one client with 100,000 offline edits, slow consumers, saturated persistence queues, interrupted asset upload and revoked offline editor. Failure of a limit must return a recoverable state without dropping locally saved work.
- [ ] Kill Go before/after each persistence boundary, restart CouchDB and interrupt network connections; compare every acknowledged receipt against recovered state. Run randomized operation permutations and verify all authorized replicas converge after quiescence, including restart across snapshots.
- [ ] Profile and fix measured bottlenecks only. Document remaining capacity limits and exact hardware/versions. Commit evidence and harness; no release if acknowledged edits disappear, ACL leaks occur or queues grow without bound.

### Task 13: Package, operate and roll out

**Files:** Create `packaging/server/{Dockerfile,compose.yaml,config.example.yaml}`, `docs/collaboration-operations.md`; modify `scripts/release-macos.py`, `scripts/release-windows.ps1`, relevant Linux packaging and CI/build scripts discovered during execution.

**Consumes:** Passing capacity/correctness evidence. **Produces:** Reproducible deployment and desktop packages with native dependencies.

- [ ] Write a deployment smoke script for private CouchDB networking, restricted service credentials, HTTPS/WebSocket upgrade, graceful Go replacement, readiness after replay and restoring a backup into a fresh environment.
- [ ] Package the pinned native library with Qt dependencies for each desktop platform; verify macOS signing/notarization inputs, Windows runtime DLLs and Linux linkage. Prove no developer-machine library paths leak into artifacts.
- [ ] Implement configuration and metrics for rooms, queues, persistence latency, merge time, replay duration, permission failures and active sessions; redact bearer tokens, invite tokens and map text. Separate liveness from readiness. Configure process limits, maximum HTTP bodies, timeouts, compaction and backup schedules consistently with tested sizes.
- [ ] Document single-active-process replacement with no rolling overlap, loss of availability during restart, CouchDB recovery, snapshot verification, disk exhaustion, key rotation and cache/revocation behavior. No horizontal autoscaling until ownership/fencing is separately implemented.
- [ ] Run `scripts/test-collaboration.sh`, the full existing CTest suite, platform package smoke tests and a backup/restore drill. Roll out behind a collaboration feature flag to two test accounts first. Preserve local-file mode if the backend is unavailable.
- [ ] Commit release documentation and write the required timestamped progress summary. Deployment/publication is a separate action from implementing this plan.

## Completion criteria

- Two accounts can share a map, edit the same text and structure live, and observe authenticated presence.
- Both can edit offline, restart, reconnect and converge automatically under the documented tree/deletion rules.
- Current permissions gate every server delivery/upload; revocation preserves private local edits without publishing them.
- Every durable receipt survives service restart and backup restore; no unacknowledged/orphan batch is mistaken for committed content.
- Local files, supported rich formatting, image/document portability and existing platforms remain supported.
- Performance targets have measured results, with no correctness tradeoffs hidden as optimizations.

## Execution handoff

Review the plan and design together. Recommended execution is task-by-task with a review gate after the native compatibility milestone, the durable commit/authorization milestone and the desktop offline milestone. Native execution or subagent-driven execution can follow the same dependencies; do not parallelize coupled schema/adapter/commit changes before their interfaces are stable. Implementation has not started.
