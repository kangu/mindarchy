# ShareClient HTTP layer

## Initial Prompt
Implement Task 2 of the client-sharing plan: `ShareClient` (QNetworkAccessManager HTTP layer for auth, maps, invites) with an inline loopback `QTcpServer` stub, per `.superpowers/sdd/task-2-brief.md`, executed via subagent-driven development.

## Plan
Task 2 of `docs/superpowers/plans/2026-09-20-client-sharing-endtoend-plan.md`: TDD — stub server + failing test, implement `src/collaboration/shareclient.{h,cpp}` (login with AuthSession cookie jar, `GET /v1/me`, `GET /v1/maps`, `POST /v1/maps`, invites + accept invite with `GET /v1/maps/{id}` hydration), register offscreen fast test, review, fix reviewer finding.

## Implementation Summary
Added `ShareClient` with the exact signal contract for Task 4 (`signedInChanged`, `loginFailed`, `mapsReady`, `mapStateReady(mapId,state,seq)`, `mapCreated`, `inviteSent`, `error`), 5 s per-request transfer timeouts, Go-native JSON field parsing (`ID`/`Snapshot`), and base64 snapshot caching. Review found `acceptInvite` reusing `mapCreated` — fixed by adding a dedicated `inviteAccepted(mapId, role)` signal plus login/accept robustness guards; type-loose test compares made explicit. 6/6 stub-server tests pass; `dev.py check` fast suite green. Also committed leftover prior-session work (share toolbar relocated into the document workspace with `share.svg`, fb60904) to keep the tree clean, and wrote this summary.

## Next Steps
Task 3 `ShareTransport` (QWebSocket + strict Submit envelope), then Task 4 `ShareCoordinator` consumes ShareClient signals.
