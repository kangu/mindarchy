# Collaboration implementation review — 2026-09-20

## Verdict

The working tree contains useful protocol, Automerge and persistence experiments, plus a partial Go HTTP/WebSocket backend. It does not yet implement the planned sharing feature and is not safe to expose to multiple accounts. The native compatibility gate is incomplete; production live fanout, full offline editing and Qt integration are absent. No plan task can be marked fully accepted against all of its stated criteria on the evidence available.

Scope: current tracked and untracked sources, compared with `docs/superpowers/plans/2026-09-20-go-couchdb-collaboration.md` and its supporting design. The assessment covers source behavior rather than the untracked prebuilt `backend/mindarchy-server` binary. Review only; no implementation fixes were made.

## Correctness findings

### 1. [P1] An authorized socket can write to an unauthorized map

Location: `backend/internal/httpapi/server.go:166–171`; `backend/internal/rooms/manager.go:43–65`.

The WebSocket join checks the URL map's role, but submit passes the payload's map ID to the manager without binding it to the authorized room. The manager never checks the target head's ACL and even creates a missing head. An owner/editor of map A can submit an edit targeting known map B, belonging to another account. The bytes enter B's durable chain.

Reproduced through the production HTTP/WebSocket handler in a temporary source copy with an in-memory CAS store: an owner socket on its own map received `committed` for an unrelated victim map. Required fix: bind the room ID at the transport boundary and enforce current authorization inside the coordinator for every read/write/control operation.

### 2. [P1] Production submissions bypass protocol and CRDT validation

Location: `backend/internal/httpapi/server.go:166–171`; `backend/internal/rooms/manager.go:65`.

Production uses plain `json.Unmarshal`, not `protocol.DecodeSubmit`. It also never loads/forks/applies/validates an Automerge document before persisting the payload. The decoder and projection tests exercise disconnected helpers. A version of 999, invalid device ID, zero counter, incorrect hash and non-Automerge bytes were all accepted in the isolated production-route reproduction. Such a batch can later make `couch.Replay` fail. Map creation likewise accepts arbitrary bytes rather than a validated collaboration snapshot, so its snapshots need not be loadable by replay.

The existing real-CouchDB test explicitly expects success for `Changes: []byte{0}` with a zero-filled incorrect hash (`backend/integration/couch_websocket_test.go:69`), entrenching this defect. Required fix: one shared strict ingress path, validated initial snapshots, candidate CRDT application/schema checks and no durable acknowledgment on invalid/missing-dependent changes.

### 3. [P1] Idempotency does not survive restart or uncertain writes

Location: `backend/internal/rooms/manager.go:36`, `:65`, `:81–84`; `backend/internal/couch/replay.go:12–17`.

Device/counter hashes exist only in memory. Stored batches omit authenticated account, device, counter, hash and original receipt metadata. Recreating the manager and retrying the same submission advanced the sequence from 1 to 2 in an isolated test. The same issue arises when CouchDB accepts the head write but its response is lost. Different bytes using a previously accepted counter can also escape detection after restart.

Automerge may deduplicate identical CRDT changes when eventually applied; that does not repair the wrong receipt/sequence, duplicate storage or acceptance of counter reuse. Required fix: durable receipt metadata, replay-based reconstruction and read-after-ambiguous-write reconciliation.

### 4. [P1] Accepted invitations and grants disappear after restart

Location: `backend/internal/sharing/service.go:186`, `:203–205`.

Invitations exist only in a process map. Acceptance updates only the cached ACL and deletes the token; it never CAS-updates the CouchDB head. An accepted editor loses access when the service restarts and reloads the original owner-only ACL. Reproduced using a new persistent service over the same store. Outstanding invitations also vanish. Retrying an already successful acceptance returns not-found rather than the planned idempotent result.

Required fix: durable account-bound invitation records, acceptance evidence and ordered ACL transitions through the same coordinator/head boundary as edits.

### 5. [P1] Production live editing and presence have no peer delivery

Location: `backend/internal/httpapi/server.go:145–176`.

The handler echoes presence and committed receipts only to its own connection. There is no room subscription, list of peer sockets, committed-change broadcast or snapshot/missing-change handshake. A second client connected to the same map receives neither the first client's content nor presence. The passing two-peer network test runs the separate `TestServer` and `rooms.Room` HTTP polling implementation, not this production path.

Required fix: production room membership and ordered committed-event fanout, transient presence expiry and two genuine production WebSocket clients in the acceptance test.

### 6. [P1] Production reads return the original snapshot, not accepted edits

Location: `backend/internal/sharing/service.go:117`, `:135–151`.

`Get` returns the cached creation snapshot. Hydration reads only the head's snapshot ID and never replays committed batches. The manager appends updates without refreshing that snapshot, and no production code calls `couch.Replay`. A client fetching the map after acknowledged changes sees old content, including after restart. The batches remain stored, so this is an inaccessible/stale-state defect rather than evidence that their bytes were physically deleted.

Required fix: reconstruct authoritative state from head/snapshot/reachable updates and expose it through the reconnect/read path.

### 7. [P1] Authorization expires only at socket creation

Location: `backend/internal/httpapi/server.go:130–158`.

Identity and role are captured before the receive loop. Expired authentication is not rechecked, and role changes cannot invalidate an existing socket. An editor remains able to submit after its token/session expires. Role downgrade/removal endpoints and coordinator-driven session revocation are not implemented, despite being required by Tasks 6–7. Required fix: expiry-aware sessions plus serialized permission transitions, current authorization on submissions and disconnect/removal from fanout.

### 8. [P2] Retrying an old submission returns a different receipt

Location: `backend/internal/rooms/manager.go:48–56`.

After U commits at sequence 1 and V at sequence 2, retrying U returns the current head sequence 2, rather than U's original sequence 1. Required fix: preserve and return the full original receipt. This is separate from restart durability and affects a running process.

### 9. [P2] Direct WebSocket reconnect fails after restart

Location: `backend/internal/sharing/service.go:157–162`.

`Role` checks only the in-memory map cache. The production live route invokes it without hydration. After a restart, an authorized client reconnecting directly gets 404 until another map GET/list happens to warm the cache. Required fix: load/check the authoritative head on cold room join.

## Completeness against the 13 tasks

| Task | Assessment | Evidence / remaining work |
|---|---|---|
| 1. Protocol | Partial | Strict submit decoder and tests exist. Full live handshake/dependency protocol, canonical batch format and production enforcement are missing. |
| 2. Native compatibility | Partial; gate not passed | Linux Go/C++ save/load interoperability passes. `third_party/collaboration/versions.json` explicitly lists macOS, Windows, rich marks and native undo as unverified. There is a C test shim, not the planned Qt adapter. |
| 3. Map semantics | Partial experiment | Pure Go projection handles cycles/deleted parents/order. It is not connected to Automerge state; no C++ equivalent or cross-language fixtures. Go nodes are an object map while the schema requires an array; settings are required by schema but absent from Go Model/Projection. Task/date/meeting/image/style data and monotonic deletion enforcement are incomplete. |
| 4. CouchDB store/replay | Partial | Immutable writes and head CAS exist. Replay helper has a happy-path test, but no production use, content-hash verification, sequence/head consistency, snapshot-frontier handling or conflict-leaf checking. |
| 5. Rooms/durable commits | Partial; unsafe | Per-map mutex and write-before-head-before-receipt sequence exist. No durable dedup, native candidate validation, batching, bounded worker pool/queues, memory admission or eviction. |
| 6. Accounts/sharing | Partial; unsafe | OIDC verifier, CouchDB-session fallback, create/list/read/invite/accept routes exist. Persistent invitations/grants, idempotent acceptance, role update/removal, map deletion and expiry/revocation handling are absent. CouchDB session fallback is an additional authentication design not described by the OIDC-only plan. |
| 7. WebSockets/presence | Skeleton | Join and own-receipt/presence responses exist. Peer fanout, resync, causal handshake, heartbeat expiry, slow-reader isolation and explicit transport limits are missing. |
| 8. Snapshots/assets | Not delivered | Creation snapshot storage exists. No checkpoint coordinator, asset endpoints, validated uploads, offline asset ordering, orphan policy or restore drill. |
| 9. Native offline persistence | Absent | No `src/collaboration/`, SQLite local store, PKCE login, credential integration, offline outbox or reconnect reconciliation. |
| 10. Engine/text/undo integration | Absent from app | Standalone Go undo experiment exists. Text undo uses complete-string/position comparisons, not element identities, and rejects even unrelated peer text edits. No Qt UUID mapping, incremental apply, shared editor or persisted collaborative undo. |
| 11. Sharing UI | Absent | No ShareDialog, SharedMaps, PresenceStrip, shared workspace mode or export/import integration. |
| 12. Capacity/failure testing | Not delivered | Smoke tests exist; no load driver, latency/RSS evidence, randomized convergence suite, reconnect storm or comprehensive commit-failure matrix. |
| 13. Packaging/operations | Not delivered | A server main/config exists. No server deployment package, private-network/security provisioning, native desktop packaging, observability or backup/restore runbook. |

## Performance assessment

The performance-oriented architecture is not implemented yet. Every update performs a head GET, immutable PUT and head PUT while holding a per-map mutex. There is no 25 ms batch window, in-flight worker management or bounded waiting. Room/counter caches grow without eviction. Map listing scans all map-prefixed documents, hydrates snapshots and returns full map copies. Projection repeats parent walks and scans child candidates per parent, giving quadratic work on large trees. These are source-level risks, not measured failures of the proposed latency targets; no throughput or memory benchmark evidence exists.

## Verification performed

- Ran `scripts/test-collaboration-all.sh`: native compatibility target passed; its Go output initially reused cached test results.
- Re-ran Go tests fresh with `go test -race -count=1 -v ./...` using `/home/radu/.local/opt/go/bin/go`. Local socket tests initially hit sandbox restrictions; an approved rerun passed all executed tests. The real CouchDB integration test skipped because its three required environment settings were absent.
- The fresh run exercised 14 top-level Go tests, plus the decoder fuzz seeds. Auth/rooms/sharing/httpapi have no package-local test files; some are exercised by integration tests. No extended fuzz campaign or cross-platform desktop suite was run.
- Wrote isolated review tests in a temporary copy at `/tmp/mindarchy-review-wn0gi_je/backend/integration/review_repro_test.go`, leaving repository implementation unchanged. Tests assert required behavior and demonstrate cross-map/malformed acceptance, restart dedup failure, accepted-grant loss, incorrect duplicate receipt sequence and cold-role lookup failure. Storage in these repros is an in-memory CAS implementation; production HTTP/WebSocket code is used for the cross-map reproduction. These do not replace a real-CouchDB fault campaign.
- Reviewed the plan and native-gate notes. The gate notes accurately acknowledge important missing capabilities, but the production route nevertheless accepts opaque updates.

## Recommended next work

First close the production authorization/validation holes and make the acceptance tests use the production route. Complete the native merge/rich-text/platform gate before extending opaque-payload transport. Then implement durable receipts, ordered persisted sharing permissions, authoritative replay and genuine two-peer fanout. Only after those are correct should the desktop offline layer and sharing UI be built. Keep the current backend classified as an internal prototype until those gates pass.
