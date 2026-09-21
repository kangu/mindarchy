# Client sharing final-review fixes

## Initial Prompt

Fix all final-review findings for the "Client sharing end-to-end" feature on
branch codex/evaluate-sharing: C1 own-commit echo detection via receipt
deviceId; C2 server-side invite display-name resolution; C3 invitee join flow
in QML (accept-invite, refresh shared maps, SharedMaps popup); I4 scheme-less
share-server presets; I5 websocket vs http.Server deadlines; I6 creation
snapshot validation; I7 non-persistent --share-server override; I8 untrack the
13 MB backend/mindarchy-server binary. Append a consolidated fix report to
.superpowers/sdd/task-10-report.md. No code comments; do not restart the
user's app; conventional commits grouped sensibly.

## Plan

1. Backend C2: extract auth.AccountIDForName (exact CouchSession derivation),
   add sharing.Service resolver callback, wire main.go, integration tests
   (invite by name through HTTP; hash invite still works).
2. Backend I5: zero read/write deadlines after websocket.Accept via
   http.NewResponseController; add regression pin with 2-second-timeout
   http.Server on net.Listen.
3. Backend I6: 1 MiB cap + json.Valid on map creation → 400
   {"error":"invalid_message"}; ingress tests.
4. Client C1: handleCommitted compares receipt deviceId, fake mirrors server
   shape (sender acct_, deviceId own).
5. Client I4: normalizeShareServerUrl (localhost/127.*/192.* → http, else
   https) in both setBaseUrl paths + accessor and tests.
6. Client C3: coordinator acceptInvite/refreshSharedMaps/sharedMaps property;
   ShareClient map summaries + virtual maps/acceptInvite; ShareDialog join
   row + shared-maps section + SharedMaps popup instance; coordinator tests.
7. Client I7: process-global override in ShareSettings, no QSettings write,
   test.
8. I8: git rm --cached backend/mindarchy-server + .gitignore entry.
9. Validate: go vet/test -race, dev.py build/check/ui; write reports.

## Implementation Summary

- Commits: bc6f337 fix(backend): resolve invite display names to account ids;
  aafda70 fix(backend): zero live websocket deadlines and validate creation
  snapshots; 1a6d005 fix(client): normalize share server urls and keep
  --share-server process-local; fee1a34 feat(client): invitee join flow with
  own-echo and bare-host preset fixes; fefb0be chore: untrack built
  mindarchy-server binary.
- Go 1.25 net/http already zeroes deadlines at hijack (conn.hijackLocked),
  so the I5 explicit zeroing is belt-and-braces; the 2s-timeout regression
  test pins the guarantee (3.01 s, deterministic).
- All new tests green: invite-by-name ×2, live-deadline ×1, snapshot
  ingress ×2, scheme-less shareclient ×2 + sharetransport ×1, coordinator
  accept-invite + refresh-shared-maps ×2, sharesettings override ×1.
- Validation: go vet clean; go test -race -count=1 ./... all ok; dev.py build
  19.1 s; dev.py check 8/8; dev.py ui 1/1 (both runs). Full details with
  per-fix commands in .superpowers/sdd/task-10-report.md.
- Keyboard shortcuts unchanged; user's running app not restarted.

## Next Steps

- Manually smoke-test the two-instance invite-by-name flow against docker
  CouchDB + the rebuilt server (controller decision).
- Consider Qing presence display names instead of acct_ hashes for UI.
- Optionally shrink the fast-suite wall time dominated by the sharetransport
  presence heartbeat timing test.
