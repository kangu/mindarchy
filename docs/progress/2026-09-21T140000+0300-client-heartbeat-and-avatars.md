# Client heartbeat and avatars

## Initial Prompt

Implement Task 10 client-side portions: 5 s presence heartbeat in
ShareTransport (per the Task 8 protocol note, shorter than the 10 s peer
TTL) with a wire-level test against the stub websocket server; string-entry
avatar support (initials + deterministic pastel color) in PresenceStrip.qml;
"Desktop client sharing" documentation in collaboration-vps-guide.md; and
E2E smoke readiness (backend go tests + server binary build, dev.py build /
check / ui) without starting the CouchDB/server smoke, which the controller
will run.

## Plan

1. Add `m_presenceTimer` (5 s, child) to ShareTransport; start on
   `handleConnected`, stop on disconnect and `leave()`; tick sends
   `{"type":"presence"}` when connected and mapId non-empty; keep ping timer.
2. Add `presenceHeartbeatEveryFiveSeconds` test: presence frame observed
   within 6500 ms after join; no presence after leave. Run via ctest.
3. Rework PresenceStrip.qml delegates for string entries (2-char uppercase
   initials, 31-hash into 8-pastel palette), preserving object-shape support.
4. Docs: add Desktop client sharing section + refresh stale "not yet wired"
   mentions in the VPS guide; leave README untouched (no sharing mention).
5. Verify: commit split (feat / docs), backend `go test -count=1 ./...`,
   `go build ./cmd/mindarchy-server`, `dev.py build`, `check`, `ui`.
6. Write task-10-report.md and this progress doc; do not restart the app.

## Implementation Summary

- sharetransport.{h,cpp}: heartbeat timer added (separate from ping),
  lifecycle tied to connect/disconnect/leave; new private `sendPresence()`.
- sharetransport_test.cpp: heartbeat test added (≤6.5 s presence arrival;
  silence after leave); sharetransport suite passed in 12.31 s standalone.
- PresenceStrip.qml: supports QList of strings and legacy object entries,
  deterministic color via 31-multiplier hash over fixed 8 pastel colors,
  no comments.
- collaboration-vps-guide.md: new "Desktop client sharing" section (presets,
  --share-server, CouchDB dev-account login, invite flow, auto heartbeat)
  and refreshed two stale paragraphs.
- Report: .superpowers/sdd/task-10-report.md (TDD evidence inside).

## Verification

- backend: `go test -count=1 ./...` ok (integration, collab, couch, protocol);
  `go build ./cmd/mindarchy-server` builds on darwin.
- dev.py build: completed 16.2s. dev.py check: 100% 8/8 fast tests passed
  (30.0s, no flake). dev.py ui: 100% 1/1 passed (116.4s, no flake).
- Commits: `3331c92` feat: presence heartbeat and avatar identities;
  `70833c9` docs: client share setup for the vps guide.

## Next Steps

- Controller: start debug CouchDB + local Go server (or VPS deployment),
  launch two desktop instances, log in with two CouchDB accounts, share and
  accept an invitation, verify mutual presence roster and committed sync.
- Optional follow-ups: make presence interval injectable for faster tests;
  re-evaluate presence frame trust/validation server-side; consider README
  mention of sharing when the feature stabilizes for end users.
