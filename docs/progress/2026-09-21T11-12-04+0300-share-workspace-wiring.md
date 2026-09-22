# Share workspace wiring (ShareCoordinator + CLI override)

## Initial Prompt

Implement Task 5 of the sharing plan: one `ShareCoordinator` per window bound to the workspace's `Engine`, exposed to QML as context property `share` (both `main.cpp` plain path and `macapplication.cpp` multi-window path), apply the `--share-server <url>` CLI override via a temporary `ShareSettings` before any coordinator construction (persisting fine — main path is the settings store; `macapplication.cpp` uses persisted settings only), implement the deferred cookie passthrough NOW: `ShareCoordinator::wireCookie()` connected to the client's `signedInChanged()` pulling the `AuthSession`-named cookie from `ShareClient`'s `QNetworkCookieJar` (new `cookieJar()` accessor) into `transport->setSessionCookie()`. No QML dialog rewiring (Task 6); ui_test keeps invite-disabled assertion; verify via `dev.py build` / `check` / targeted `ui`.

## Plan

1. TDD the cookie passthrough: failing test in `sharecoordinator_test.cpp` (real client jar + FakeTransport capture) → `ShareClient::cookieJar()`, virtual `setSessionCookie`, `wireCookie()` in the coordinator.
2. Wire `main.cpp`: QCommandLineParser option `--share-server`, persist override via temporary `ShareSettings` after `parser.process`, stack coordinator + root context property `share` before `qml.load`.
3. Wire `macapplication.cpp`: host-context coordinator in `createHost` (parented to host window) + per-document coordinator on `MacDocumentWindow::context` (the workspace's actual creation context — survives item reparenting by design).
4. Mirror plain-path wiring in `tests/ui_test.cpp` with a resolution assertion (`typeof share !== 'undefined' && share.signedIn === false` evaluated inside the workspace context); link `mindmap_collaboration` into `ui_test`.
5. Verify: `dev.py build`, `dev.py check`, `dev.py ui`; offscreen `--share-server` smoke run; commit + reports.

## Implementation Summary

Commit `260b7dc feat: wire share coordinator into workspace windows` (9 files, +61/−2):
- `ShareCoordinator::wireCookie()` (sharecoordinator.{h,cpp}): connected to the client's `signedInChanged` **before** `handleSignedIn` so the `AuthSession` cookie is on the transport before any join; clears the transport cookie when absent (sign-out/failed login). Also invoked once at construction.
- `ShareClient::cookieJar()` accessor (shareclient.h); `ShareTransport::setSessionCookie` virtualized for test capture (sharetransport.h).
- `main.cpp`: `--share-server <url>` parser option ("Server URL override for sharing") + `shareServerOverride(argc, argv)` fallback, persisted via temporary `ShareSettings` before both window paths; plain path exposes stack coordinator as root context property `share`.
- `macapplication.cpp`: `createHost` sets `share` (coordinator parented to the host window) on the host context; `MacDocumentWindow::load` sets a per-document coordinator (`&engine`, parented to the document) on the workspace's creation context — reparenting (merge/detach) never changes QML context resolution, so the workspace always sees its own engine's coordinator.
- `tests/sharecoordinator_test.cpp`: new `signInPropagatesAuthSessionCookieToTransport`; `tests/ui_test.cpp`: share context property + workspace-context resolution assertion, invite stays disabled; `CMakeLists.txt`: ui_test links `mindmap_collaboration`.

Verification: `dev.py check` 8/8 fast tests; `dev.py ui` `100% passed, 0 failed out of 1` (107.4 s, first run); `dev.py build` clean; offscreen `--share-server` run exits 0 with settings state confirmed unmodified. Running app not restarted.

## Next Steps

- Task 6: make `ShareDialog` live (bind to `share.*` — server field, sign-in, share/invite actions) and the presence strip; remember the Help → Keyboard Shortcuts table if any shortcut changes.
