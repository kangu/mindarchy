# Task 4 Report: ShareCoordinator (session fusion + engine glue)

## Status: DONE

- Commit: `94d732f feat: share coordinator fusing engine, outbox and transport` (branch `codex/evaluate-sharing`)
- `dev.py check`: 8/8 fast tests pass (incl. new `sharecoordinator`, ~2.3 s, stable across 3 repeats)
- `python3 scripts/dev.py build` for the app restarted separately (task says not to restart the app).

## Implementation surface

`ShareCoordinator` (src/collaboration/sharecoordinator.{h,cpp}):
- Properties: `serverUrl` (delegates to `ShareSettings`), `signedIn`, `accountName`, `shareStatus` ("Offline" / "Signed in" / "Live" / "Syncing" / "Offline · N queued"), `presence`, `mapId`.
- Q_INVOKABLE: `chooseServer`, `signIn`, `signOut`, `shareCurrentMap`, `inviteOnMap`, `joinSharedMap`, `disconnectSharing`; internal test seam `attachMapId` and `deviceId()`.
- Test seam: second constructor `ShareCoordinator(Engine*, ShareClient*, ShareTransport*, ShareSettings*, parent)`; real defaults created when nullptr.
- Snapshot model (deliberate, documented): coordinator captures `Engine::changed` (coarse signal) and debounce-arms a 300 ms single-shot; **nothing calls `CollaborationEngineBridge::recordLocalTransaction` — no engine instrumentation for per-edit records exists**, the bridge is used only for begin/end remote-apply suppression. During a remote apply `applyingRemote` swallow/report prevents re-arming.
- Upload: counter = `++m_deviceCounter` per coordinator, sha256-hex of `documentBytes()`, `session.queueChange` then `transport.submit` when connected.
- Download: `committed` with foreign sender → `beginRemoteApply` → `loadDocumentBytes(state, documentPath())` → `endRemoteApply` → `session.saveLocal(state, seq, counter+1)`. Own echo clears the pending dedupe state (and pending row via `CollaborationSession::clearPendingByCounter`).
- Outbox: on `joined`, drain `pendingForSubmit()` oldest-first via transport submit.
- Rejections: `counter_reuse` → `++counter`, requeue same bytes, resubmit (mitigation for server review #3); `access_revoked` → `session.markAccessRemoved()` + leave; `resync_required` → `client.fetchMapState`.
- Sidecar: `<documentPath>.share` JSON `{serverUrl, mapId, role, account}`; read back at construction, auto-join when signed in; `disconnectSharing()` removes it.

## Small neighboring changes (required, noted)

- `Engine`: moved `documentBytes()` / `loadDocumentBytes()` from private to public (the brief lists them as the consumed Engine API; they were private).
- `ShareClient`: `setBaseUrl`/`login`/`createMap`/`fetchMapState`/`signedIn()`/`accountName()` now virtual so tests can inject behavior-only fakes; `ShareTransport` gained virtual `join`/`leave`/`connected()`. `submit` was already virtual.
- `CollaborationSession`: added `pendingForSubmit()` and `Q_INVOKABLE clearPendingByCounter()` for tracking and removing pending rows by counter.
- CMake: `sharecoordinator.cpp/.h` in `mindmap_collaboration` (which now links `mindmap_engine` PUBLIC because of the Engine references — needed by sharesettings/shareclient/sharetransport fast tests that compile the collaboration lib); `sharecoordinator_test` target (Qt6::Network + WebSockets + Test + Qt6::Test) and `fast_tests += "sharecoordinator"`.

## Test fixture correction

Hand-rolled doc from the brief is invalid (Engine requires layout `Horizontal|Vertical|Compact`, spacing `Narrow|Standard|Wide`, `branchStyle` ∈ MapDrawing::branchStyles(), `themeId` in Themes, ≥1 node). Test fixture corrects it to `{format:"mindarchy", version:1, layout:"Horizontal", spacing:"Standard", branchStyle:"Rounded", themeId:"lab", manual:false, nodes:[exact field set], connections:[]}` and compares round-tripped bytes through a second `Engine` (the engine serializes extra fields).

## Concerns / notes

- `TextMeasure` outbox stores use `DocumentSession::defaultDirectory()`; tests enqueue rows under account `test-account`/unique map ids in the user session dir (SQLite collaboration.sqlite). Acceptable (same mechanism real app uses), noted as pollution.
- Session cookie propagation from ShareClient's jar to the transport is not wired (no getter in ShareClient) — left documented TODO for Task 5/6 surface polish.
- Own-echo detection uses the transport's emitted `sender`; if the backend echoes the wire deviceId, coordinator deletes pending by that id.
