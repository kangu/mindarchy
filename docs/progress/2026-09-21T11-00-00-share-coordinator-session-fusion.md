# ShareCoordinator engine fusion

## Initial Prompt
Implement Task 4 of the client-sharing plan: `ShareCoordinator` fusing `Engine`, `CollaborationSession`/outbox, `ShareClient`, and `ShareTransport` (snapshot payloads, debounce, sidecar attachment), per `.superpowers/sdd/task-4-brief.md`, executed via subagent-driven development.

## Plan
Task 4 of `docs/superpowers/plans/2026-09-20-client-sharing-endtoend-plan.md`: TDD with injectable fake client/transport; QML surface (serverUrl, signedIn, accountName, shareStatus, presence, mapId); debounced whole-document upload; remote apply via `CollaborationEngineBridge`; counter_reuse requeue; outbox drain on join; sidecar `<documentPath>.share`.

## Implementation Summary
`ShareCoordinator` landed with dual constructor (test seam) and six offscreen tests. Review caught a critical device-identity mismatch (coordinator UUID vs transport UUID breaking own-echo dedupe → pending growth): fixed with `ShareTransport::setDeviceId` wired from the coordinator, own-echo test now exercised. Also fixed: counter_reuse removes the rejected row before requeue and caps retries (2 submits per payload, then "Sync error (counter_reuse)"), peer commits no longer pollute pending state, disconnectSharing emits mapChanged, queued count shows pending rows, and drained rows reset the attempt cap (cde7c31). `dev.py check` 8/8; coordinator + transport suites stable.

## Next Steps
Task 5 wires the coordinator into `DocumentWorkspace`/`main.cpp` (`--share-server` override, cookie passthrough from client jar) and Task 6 makes the ShareDialog live.
