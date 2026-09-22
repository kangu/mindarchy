# ShareTransport (WebSocket + Submit envelope)

**Date:** 2026-09-21T10:10:00
**Commit:** `2dcc0f2 feat: share websocket transport with submit envelope`

## Initial Prompt

Task 3 of the sharing plan: implement `class ShareTransport : public QObject` in `src/collaboration/sharetransport.{h,cpp}` with Q_INVOKABLE `setBaseUrl`/`join`/`leave`/`submit` (virtual) plus `setSessionCookie`, properties `connected`/`errorCode`, signals `joined`/`committed`/`presence`/`rejected`/`connectedChanged`, static `encodeSubmit` producing the submit envelope with sha256 hex hash and base64 changes, reconnect backoff (1000 ms doubling, 30 s cap, reset on connect), 15 s keepalive ping, cookie-authenticated handshake, presence single-entry tolerance. TDD via in-test `QWebSocketServer`; CMake target `sharetransport_test` in fast tests; link `Qt6::WebSockets`.

## Plan

1. Study Task 1-2 patterns (`ShareClient`, `shareclient_test.cpp`), CMakeLists, wire fixture.
2. Write `tests/sharetransport_test.cpp` covering envelope shape/hash/base64, live-URL join + hello, submit over wire, committed state decode, rejected passthrough, presence roster+merge, join-switch leaves first.
3. Register CMake (target, ctest, fast_tests, WebSockets dependency).
4. Verify RED, implement, verify GREEN.
5. `dev.py check` + `dev.py build`, restart app, commit, write report.

## Implementation Summary

- `src/collaboration/sharetransport.{h,cpp}`: full transport per contract. `submit` is `Q_INVOKABLE virtual` and sends the envelope using the caller-provided hash verbatim; `encodeSubmit` computes sha256 (`QCryptographicHash::Sha256`) over raw changes bytes. Reconnect via single-shot `QTimer` (1000→30000 ms doubling, reset on connected), `QWebSocket::ping()` every 15 s, cookie via `QNetworkRequest::CookieHeader`, `http(s)`→`ws(s)` URL normalization, per-instance `deviceId` (QUuid, no braces).
- `tests/sharetransport_test.cpp`: 7 cases against a localhost `QWebSocketServer` stub; all pass.
- `CMakeLists.txt`: `WebSockets` added to find_package and `mindmap_collaboration`; `sharetransport_test` target/test registered; `fast_tests += sharetransport`.
- Environment: Qt 6.11.2 prefix lacked QtWebSockets; built qtwebsockets from source at installer SHA `3eab06de…` and installed into `/Users/radu/Qt/6.11.2/macos`.
- Notes: test fixture hash corrected to verified sha256("hello") `2cf24dba…`; cookie asserted via `rawHeader("Cookie")` (typed CookieHeader QVariant not populated server-side).

## Verification

- RED confirmed (undeclared `ShareTransport`) before implementation.
- `ctest -R '^sharetransport$'`: 100% pass. `dev.py check`: 7/7 pass. `dev.py build`: success. App restarted.

## Next Steps

- Task 4 (ShareCoordinator): consume ShareTransport, wire cookie sharing from ShareClient's `QNetworkCookieJar`, compute changes hash and call virtual `submit`.
