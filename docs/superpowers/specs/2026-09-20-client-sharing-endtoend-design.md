# Client sharing end-to-end (server chooser + live sync) — design

Date: 2026-09-20
Status: Approved design (sections approved interactively)
Scope: Qt prototype (`qml/`, `src/collaboration/`) + Go backend blockers (`backend/`)

## Goal

Real end-to-end sharing from the Qt client to the existing Go+CouchDB server: sign in, choose which server to use (`share.mindarchy.xyz` preset or `http://localhost:8080` preset, plus custom URL), invite accounts as editor/viewer, and collaborate live (two-way editing) while keeping the existing offline outbox path intact.

User decisions recorded during brainstorming:
- Real end-to-end sharing (not a stub/status-only milestone).
- Authentication: CouchDB dev accounts through the server's `<span class="math-inline">\_session</span>` flow (full OIDC later, this iteration is dev accounts against CouchDB).
- Server picker lives directly in the Share dialog, persisted via QSettings, with a `--share-server` override.
- Editing model: two-way live editing via snapshot-payload transactions (concurrency resolves last-writer-wins; see Payload model below).
- Server-side work: fix only what blocks the client path (P1 #1, #2, #4, #5, #6, #7 from `docs/collaboration-implementation-review-2026-09-20.md`), leaving #3 (durable receipt metadata across restart) and the P2 findings documented as known issues.

## Architecture

```
┌──────────────────── Qt client (this task) ────────────────────┐
│ ShareDialog (server picker, login, invite, live status)        │
│        │                                                       │
│ ShareCoordinator (C++, one per window/session)                 │
│  • ShareSettings   – server preset picker + QSettings persist  │
│  • ShareClient     – HTTP: account, maps, invites (QNetwork)   │
│  • ShareTransport  – WebSocket to /v1/maps/{id}/live           │
│        │                                                       │
│ CollaborationSession (exists) – status/mapId/role              │
│  • CollaborationLocalStore (exists) – offline SQLite outbox    │
│  • CollaborationEngineBridge (exists) – remote-apply gating    │
│        │                                                       │
│ MindCanvas / Engine                                            │
└───────────────┬────────────────────────────────────────────────┘
                │ HTTPS + WSS (Submit envelope, protocol v1)
┌───────────────▼───────── Go backend (this task) ───────────────┐
│ httpapi server   auth (CouchDB dev accounts)                   │
│ rooms manager    sharing service (durable ACL/invites)         │
│ couch store/replay   collab validation (Automerge)             │
│ CouchDB                                                       │
└────────────────────────────────────────────────────────────────┘
```

Key decisions:
- One new C++ class `ShareCoordinator` per open document owns `CollaborationSession` + network; exposed to QML as a context property so the existing stub dialog becomes live.
- **Payload model (confirmed 2026-09-20): snapshot payloads.** The Engine has no Automerge change emission (nothing calls `CollaborationEngineBridge::recordLocalTransaction` in `src/engine` today; the engine exposes `documentBytes()` / `loadDocumentBytes()` whole-document serialization only, and the native Automerge gate is unverified on macOS). Each collaborative transaction therefore carries the whole Engine document (engine `documentBytes`); the server validates the envelope (strict protocol decode, SHA-256 hash, size ≤ 1 MiB, valid JSON document body) and persists it as a batch — no Automerge apply. Concurrency converges last-writer-wins per transaction. The Submit envelope (version, mapId, deviceId, counter, hash, changes) is unchanged so a future Automerge-engine swap replaces payload decoding without touching the transport/room/ACL surface. This is a deliberate deviation from the review doc's Automerge ingestion gate; known-issue list updated below.
- Offline map first: the existing SQLite outbox persists accepted state and queued changes; when the socket is up, the outbox drains in order.
- Server choices ship as two presets (`share.mindarchy.xyz`, `http://localhost:8080`), stored via `QSettings("Mindarchy")` (`AppIdentity::windowSettings()`), with `--share-server <url>` as a non-persisted CLI override.

## Client components

All new C++ lives in `src/collaboration/`:

- `ShareSettings` — holds the current server preset (URL, label, secured flag), the two built-in presets plus a custom URL option; load/persist via `AppIdentity::windowSettings()` keys `sharing/serverUrl` (+ `sharing/accountId`); static helper for the `--share-server` CLI override used by `main.cpp`.
- `ShareClient` — QNetworkAccessManager wrapper: `GET /v1/me`, `POST /v1/auth/session`, `GET /v1/maps`, `POST /v1/maps`, `POST /v1/maps/{id}/invites`, accept invite, `GET /v1/maps/{id}` (hydration). Signals only (`meReady`, `signedIn`, `mapsReady`, `inviteAccepted`, `error(code,message)`); coordinator drives logic. 5 s per-request timeouts; server error codes normalized to dialog status strings. Auth = CouchDB dev-account login (name + password → session cookie; cookie rides on HTTP requests and the WS handshake).
- `ShareTransport` — QWebSocket to `/v1/maps/{id}/live`. Sends strict protocol-v1 `Submit` envelopes; receives committed receipts, peer changes, presence. Exponential reconnect backoff (1 s → 30 s cap). Pending-receipt bookkeeping keyed (deviceId, counter); dedupe repeated acknowledgments. Does not resend on its own — the coordinator resumes from the sequence cursor and drains the outbox.
- `ShareCoordinator` — one per open document (owned by the workspace). Q_INVOKABLE API for QML: `signIn(name, password)`, `signOut()`, `invite(account, role)`, `openSharedMap(id)`, `chooseServer(url)`; readable props `serverUrl`, `signedIn`, `shareStatus`. Upload path: `Engine::changed` → `CollaborationEngineBridge::localTransaction` → `session.queueChange` → transport submit when online. Download path: transport applyRequest → `beginRemoteApply()` → Engine apply → `endRemoteApply()` → `session.saveLocal`. Initial share: serialize the current document as the validated creation snapshot for `POST /v1/maps`.

Wiring: `DocumentWorkspace` (per-window) constructs the coordinator with the workspace's `Engine`; registered via `setContextProperty("share", …)` following the existing `engine` context-property pattern (`src/main.cpp:232` and per-window `src/macapplication.cpp:527-534`).

## Server-side blocker fixes

Anchors from the review doc `docs/collaboration-implementation-review-2026-09-20.md`:

1. **#1 Cross-map writes** (`backend/internal/httpapi/server.go:166-171`, `backend/internal/rooms/manager.go:43-65`): bind the socket to exactly one room at the transport boundary (the authorized URL map). The socket path never consults a payload `mapId` for routing — payload map ID must equal the bound map or is rejected. The coordinator refuses create/step for any head other than the bound one and never implicitly creates a head.
2. **#2 Validation** (`server.go:166-171`, `manager.go:65`): production submit keeps `protocol.DecodeSubmit` (strict envelope: version, mapId binding, hash, size, counter), then persists only a batch whose `changes` payload is a valid JSON document body (snapshot-payload model above; Automerge candidate application is explicitly NOT performed in this iteration). Map creation requires a JSON-decodable snapshot. `backend/integration/couch_websocket_test.go:69` (currently asserting success for `{0}` bytes with a zero hash) is inverted to assert rejection with the proper machine-readable code.
3. **#4 Durable invitations/grants** (`backend/internal/sharing/service.go:186`, `:203-205`): invitations and acceptance evidence become CouchDB documents bound to the map+account, updated through the same head-CAS transition edits use. Restart rehydrates invitations, ACLs, and acceptance; repeated acceptance is idempotent (not `not-found`).
4. **#5 Peer fanout** (`server.go:145-176`): the production WebSocket handler installs registry-driven room membership keyed by the bound room; committed events (ordered by sequence) broadcast to all joined sockets; presence entries carry a heartbeat-refreshed TTL and expire silently. The two-peer integration test moves to the genuine production WS route (no longer the `/v1/test` harness).
5. **#6 Stale reads** (`sharing/service.go:117`, `:135-151`): `Get`/hydration replays head → batches via `couch.Replay` (bounded, cache invalidated by head version) instead of returning the creation snapshot. Acknowledged edits are visible after `committed`, including after restart.
6. **#7 Auth expiry** (`server.go:130-158`): sockets recheck session validity on submit and heartbeat; on expiry/role change the server closes with `access_revoked` and the coordinator drops the peer from the room. Durable role transitions (from #4) drive live revocation.

Explicitly out of scope (documented as known issues, later task): #3 durable receipt metadata across restart, the P2 retry-receipt mismatch, P2 cold-room role lookup fix (partially addressed by #6 hydration), and all other P2/capacity/ops items.

## UI changes

- `qml/ShareDialog.qml` becomes live, bound to the `share` context property:
  - Server row: ComboBox of the two presets + "Custom…" (URL text field) + Connect.
  - Sign-in block: account, password, Sign in button, status label; swaps to "Signed in as ⟨name⟩ · Sign out".
  - Sharing block (enabled only when signed in): invite account TextField, editor/viewer ComboBox, Invite button; plus a "Shared with me" list fed by `GET /v1/maps` — instantiating the currently-orphaned `qml/SharedMaps.qml`.
  - Status line bound to `shareStatus` (e.g. "Live · 3 editors", "Offline · 2 changes queued").
- `qml/PresenceStrip.qml` (currently orphaned) is instantiated above the canvas and shows live participants for shared maps.
- Share button (document toolbar) gains a sync-state dot: gray local, amber pending, green live. No new global shortcut is bound in this iteration; if one is added, the Help → Keyboard Shortcuts table (`src/macwindow.mm` / `qml/KeyboardShortcuts.qml`) must be updated in the same change.
- Connection failures surface after 3 attempts; transport loss is a status line, never a modal.

## Data flow

First-time share (owner): connect+sign in → `POST /v1/maps` with validated engine snapshot → server commits creation snapshot → `mapId` persisted into the local map metadata → client joins `/live` for presence and fanout.

Editing online: Engine change → bridge localTransaction (gate-aware) → outbox durable write → WS Submit → committed receipt → session "Live". Peer edits: receive → `beginRemoteApply` → Engine apply → `endRemoteApply` → `saveLocal` durable row.

Offline/disconnected: submissions land in the outbox at queue time; transport retries with backoff; on reconnect the client hydrates via `GET /v1/maps/{id}` (server replay state) and drains the outbox oldest-first. `counter_reuse` (e.g. crash without receipt) is handled client-side by re-enqueueing the same bytes with a fresh counter — durable server-side receipt metadata is the documented known issue (#3). `resync_required` forces a full reload-from-server rather than silent divergence.

Local-only maps remain unchanged; sharing attaches only via the dialog.

## Error handling

- Server codes map to a single `ShareStatus` enum → dialog text + toolbar badge; unknowns become "Sync error (code)".
- Transport loss: status "Offline (reconnecting, attempt N)"; no modals.
- Validation errors on the server produce stable protocol codes before anything durably commits.

## Testing

Client (offscreen, new `tests/sharecoordinator_test.cpp` plus a loopback HTTP+WS stub server on `QTcpServer`/`QWebSocketServer`):
- Sign-in, invite, send/receive apply, outbox drain, reconnect resume.
- Envelope encoding verified against protocol-v1 fixtures (`tests/fixtures/collaboration/wire.json`).
- `ui_test.cpp` share-dialog tests extended for the live dialog (stubbed coordinator).
- Existing `collaboration_store_test` / `collaboration_engine_bridge_test` stay green.

Server (Go):
- The review-doc repros become regression tests in `backend/integration/`: cross-map rejected, invalid bytes rejected with the stable code, accepted grant survives restart, two peers receive each other's commits on production WS, stale read gone, expired-submit revoked.
- `couch_websocket_test` inverted for the validation defect.
- `scripts/test-collaboration-all.sh` must pass; `TestRealCouchDBRestartAndDurableWebSocketSubmit` runs when CouchDB env is provided.

## Known issues (documented, not fixed here)

- **Snapshot payloads, not CRDT changes:** whole-document transactions converge last-writer-wins; per-element CRDT merge waits for an in-engine Automerge change stream (macOS native gate unverified).
- #3 durable receipt/idempotency metadata across server restart (client mitigates `counter_reuse` by re-enqueueing).
- P2: retried historical submission may return current head sequence instead of the original receipt.
- Snapshot payload element-level schema validation (per `backend/internal/collab/schema.go`) is future work; this iteration validates envelope + JSON-decodable body + size.
- Native Automerge gates for macOS/Windows remain unverified; the Linux compat gate stays the interop proof for change-byte format.
- Backend capacity/latency work per Task 12 remains future work.

## Success criteria

1. With the backend running at `http://localhost:8080` (and, with TLS, at `share.mindarchy.xyz`), a user can sign in, share a local map, invite another account as editor or viewer, and both accounts can open and live-edit the same map, seeing each other's changes within one round-trip.
2. With network dropped, edits queue locally in SQLite and sync automatically on reconnect without loss or duplication.
3. `python3 scripts/dev.py check` passes with the new tests; `scripts/test-collaboration-all.sh` passes (fresh Go tests).
4. The ShareDialog no longer shows "Online sharing is not available in this build".
