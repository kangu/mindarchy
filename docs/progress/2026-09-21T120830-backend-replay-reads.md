# Backend Replay Reads (Task 7)

Date: 2026-09-21T12:08:30

## Initial Prompt

Implement Task 7: replay-based share reads (backend blocker #6). `sharing.Service.Get/Role/Accept` hydrated only the head `SnapshotID`, so committed batches (via `rooms.Manager.Submit`) were invisible to fresh services. Per the payload-model decision (whole-document JSON snapshots, not Automerge change bytes), reconstruction uses a payload-model-agnostic rule: effective snapshot = last non-empty batch payload when a chain exists, else the stored snapshot; `Replay` (Automerge apply) stays for future Automerge payloads. Add `ReplayBatched` in couch, `ReplayChain` on the `Persistence` interface + couch `StoreClient`, hydrate-with-chain in the sharing service with an early-exit when the hydrated head still matches. Test-first with a shared in-memory CAS store integration test. Validate with `go test -race -count=1 ./...`, commit, report.

## Plan

1. RED: add `backend/integration/replay_reads_test.go` with in-memory CAS store implementing `sharing.Persistence` (+`couch.Store` for `rooms.NewManager`); create → PersistMap → Submit → fresh service Get must return the batch state; Role = owner; single-process service rehydrates. Confirmed failure (old snapshot served).
2. `internal/couch/replay.go`: extract `batchChain` helper; add `ReplayBatched` returning oldest-first batch `Changes` without Automerge application; keep `Replay` semantics.
3. `internal/couch/store.go`: `StoreClient.ReplayChain` delegating to `ReplayBatched`.
4. `internal/sharing/service.go`: extend `Persistence` with `ReplayChain`; track hydrated `head` on `Map`; new `hydrateWithChain` (head-match early exit; effective snapshot = last non-empty batch or stored snapshot); switch `Get`, `Role`, and `Accept` reconstruction to it.
5. GREEN: full `go test -race -count=1 ./...` green.
6. Commit `fix(backend): replay committed batches on share reads`; write task report and this progress doc.

## Implementation Summary

- `internal/couch/replay.go`: `ReplayBatched(ctx, store, mapID) (protocol.Head, [][]byte, error)`; shared `batchChain` does the head-down walk (cycle detection, batch sanity, reversed oldest-first). `Replay` now reuses `batchChain` and still applies changes through `collab.Document`.
- `internal/couch/store.go`: `StoreClient.ReplayChain` satisfies the extended `sharing.Persistence`.
- `internal/sharing/service.go`: `Persistence` gains `ReplayChain`; `Map.head` (unexported) enables the cheap head-match early exit; `hydrateWithChain` serves the last non-empty batch payload as the effective snapshot (whole-document model) and refreshes Owner/ACL; `Get`, `Role`, `Accept` use it; plain `hydrate` (List) records the head.
- `integration/replay_reads_test.go`: in-memory CAS harness (`memCASStore` with rev-bumping CAS and chain-walking `ReplayChain`) covering fresh-service reads, Role, and single-process rehydration.
- Validation: all backend packages ok under `-race -count=1` (pre-existing linker LC_DYSYMTAB warnings only).

## Next Steps

- Task 8: room envelope validation in `rooms.Manager.Submit`.
- Optional: snapshot compaction to bound batch chain length; adopt Automerge replay (`Replay`) once payloads become Automerge saves.
