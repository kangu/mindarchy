# Backend ingress validation, committed fanout and live reauth (2026-09-21T12:26:48+03:00)

## Initial Prompt

Task 8 of the client-sharing end-to-end plan: Go backend blockers narrowed to the client path — snapshot-payload ingress validation in `rooms.Manager.Submit` (Plan/JSON validity, 1 MiB decoded bound, hash double-check, per-map authorization via a `CheckTarget` callback with a distinct `access_denied` refusal for a nil callback), httpapi wiring of `CheckTarget` from the sharing Role/ACL, per-message identity + role reauth in the live WebSocket handler (reject `access_revoked`, close socket, remove from registry), committed distribution to all room peers including `state` (base64 change bytes) and `sender`, a TTL-based (10 s, configurable) presence roster with a background sweep broadcast on membership deltas plus a roster send right after `hello`, inversion of the accepted non-JSON submit expectation in `couch_websocket_test.go:69`, and new production-route regression tests in `backend/integration/ingress_reauth_test.go`.

## Plan

1. TDD failing tests first (production WS route over the mem-CAS harness): invalid snapshot bytes rejected with `invalid_message` and untouched head; cross-map submit locked to `access_revoked` + direct-manager `ErrAccessDenied`; expiring-session submit rejected and socket closed; presence roster broadcast on join and TTL shrink.
2. Extend `couch_websocket_test.go` to expect the invalid-bytes rejection while keeping durable coverage with a subsequent valid-JSON submit.
3. Implement `rooms.Manager` gate: `CheckTarget` first (nil ⇒ `ErrAccessDenied`), `json.Valid`/1 MiB ⇒ `ErrInvalidSnapshot`, SHA-256 ⇒ `ErrInvalidMessage`, stable-code sentinels exported; update `replay_reads_test.go` to configure `CheckTarget`.
4. Wire `CheckTarget` inside `httpapi.NewProductionServer` from `sharing.Role`; rewrite `liveHandler` for per-message reauth, error-code translation to stable wire codes, committed+state fanout to all peers, roster broadcast after hello, join/delta presence map and janitor sweep (`WithPresenceTTL` option; 10 s TTL, TTL/10 sweep cadence bounded to ≥50 ms).
5. Update `system_test.go` to skip the roster delivered after `hello`; add the `committed`/`presence` message shapes to `docs/collaboration-protocol.md`.
6. Validate `go vet ./...` + `go test -race -count=1 ./...`, commit, report.

## Implementation Summary

- `backend/internal/rooms/manager.go`: `CheckTarget` callback enforced first (nil callback ⇒ refusal); JSON validity + decoded-size gate then SHA-256/hash check before dedupe/commit; new `ErrAccessDenied`/`ErrInvalidMessage`/`ErrInvalidSnapshot` sentinels carrying stable codes.
- `backend/internal/httpapi/server.go`: `NewProductionServer` sets `CheckTarget` from `sharing.Role` (viewer/missing roles denied); `liveHandler` re-runs `s.identity(request)` every loop iteration and `sharing.Role` per submit — failures answer `rejected access_revoked` and close the socket after registry removal; committed broadcast now `{"type":"committed","receipt","state":base64,"sender"}` to all room peers; joins/deltas broadcast `{"type":"presence","accounts":[sorted]}` refreshed by any incoming message with a background sweep; rejection codes translated through `errors.Is` plus `ProtocolError` passthrough.
- Tests: new `backend/integration/ingress_reauth_test.go` (4 regression tests named above), inverted invalid-bytes expectation in `couch_websocket_test.go` (with the durable valid-submit path preserved), `replay_reads_test.go` `CheckTarget` config, `system_test.go` roster-skip.
- Docs: `docs/collaboration-protocol.md` gained the Live WebSocket messages section documenting both shapes.
- Validation: `go vet ./...` clean; `go test -race -count=1 ./...` fully green fresh. One early race (TTL set after janitor start) found by `-race` and fixed via the `WithPresenceTTL` constructor option.
- Commit: `516d660 fix(backend): snapshot ingress validation, committed distribution, live reauth`. Report at `.superpowers/sdd/task-8-report.md`.

## Next Steps

- Task 9: production two-peer end-to-end arbitration (peer A/B both join `/live`, exchange committed state both directions, survive service restart) plus the client-side transport/coordinator consumption of `state`/`sender`.
- Client `ShareTransport`/`ShareCoordinator` changes on receiving committed-with-state (Task 3 already parses the payload shape; verify against this final schema).
- Optional follow-up: per-message role recheck caching to avoid a head load per submit on the couch store path.
