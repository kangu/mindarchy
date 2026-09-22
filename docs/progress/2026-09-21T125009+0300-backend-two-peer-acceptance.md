# Backend two-peer production acceptance (Task 9)

Date: 2026-09-21T12:50:09+0300
Branch: codex/evaluate-sharing · Commit: 57ff673 `test: two-peer production websocket acceptance`

## Initial Prompt

Task 9 of the client-sharing end-to-end plan: one in-process acceptance test `TestTwoPeerProductionLiveEditing` (`backend/integration/two_peer_production_test.go`) exercising the full production path for two peers — owner creates a map via HTTP, invites editor B via the durable invite endpoints, both connect genuine production WebSockets to `/v1/maps/{id}/live`, alternating whole-document commits broadcast exactly once with base64 `state` equality, then a service restart (fresh `sharing.Service` + `rooms.Manager` + `ProductionServer` over the same CAS store) where fresh sockets rejoin and B's fresh `GET /v1/maps/{id}` returns the rehydrated document. Couch-free, <10s, `-race` clean, go vet clean.

## Plan

1. Study plan Task 9 and existing harnesses (`newLiveHarness`, `fakeCouchSessions`, `memCASStore`, restart pattern in `couch_websocket_test.go`).
2. Build a restartable harness (single httptest server, atomic handler swap to simulate the production restart over the same store; cookie identities stable).
3. Write the acceptance test: rosters on join → A commits alpha (seq 1) → B commits beta (seq 2) → exactly-once checks → teardown/restart → rejoin rosters → rehydration via HTTP GET.
4. TDD: run first, record RED/GREEN honestly; fix genuine defects in production code only.
5. Verify: vet, full `-race` suite, `scripts/test-collaboration-all.sh`, restart dev server, commit, report.

## Implementation Summary

- **RED (honest):** first run failed — first committed receipt arrived with `seq:2`, not `seq 1`. `sharing.Accept` bumped `head.Seq` during the ACL CAS (legacy line from `cf6ac93`), so accepting an invite consumed a committed-sequence number. Since `seq` is the client's committed-events cursor (`docs/collaboration-protocol.md:80-83`), ACL changes must not inflate it. Fixed `backend/internal/sharing/service.go` to preserve `Seq: head.Seq` (rev still invalidates hydration caches).
- **GREEN:** after fix, `go test -race -count=1 -run TestTwoPeerProductionLiveEditing` → `ok 1.968s`; full `go test -race -count=1 ./...` green; `go vet` clean.
- Harness detail: a deadline-expired `Read` in `coder/websocket` v1.8.15 closes the connection, so exactly-once is asserted by ordering (next read must be the next seq) plus a drain-then-`CloseNow` check on old sockets before teardown.
- `scripts/test-collaboration-all.sh`: Go portion green; compat CMake step fails on a pre-existing local Qt 6.6/icu4c@77 breakage (`rcc` can't load `libicui18n.73.dylib`) — environment issue, unrelated.
- Dev server restarted (`/healthz` alive). No comments in Go; test ~2s, Couch-free.

## Next Steps

- Task 10: local live fire (CouchDB + two app instances) and dev setup docs.
- Fix local Qt/icu4c environment to restore the compat test script.
