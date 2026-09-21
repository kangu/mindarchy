# Live Share Dialog

## Initial Prompt
Task 6: rewrite qml/ShareDialog.qml (objectName "shareDialog", Popup-modal, palette-consistent) into a live dialog on the per-document `share` ShareCoordinator context property: server presets ComboBox (model `share.presets`, "Custom…" entry + TextField) with Connect → `share.chooseServer`; sign-in/out blocks via Loaders (`share.signIn/signOut`); sharing block enabled only when signed in (`shareAccount`, `shareRole`, `shareInvite` → `share.inviteOnMap`); owner "Share this map" row (`shareShare` → `share.shareCurrentMap()` when `share.mapId` empty; map id + Stop sharing → `share.disconnectSharing()` when shared); status label bound to `share.shareStatus`; close button; remove the "not available" copy; skip the "Shared with me" section (no coordinator listing property). Add PresenceStrip (`presenceStrip`, `participants: share.presence`, visible when non-empty) at the canvas top-right in DocumentWorkspace.qml. Extend tests/ui_test.cpp share section: dialog opens, first preset "share.mindarchy.xyz", sign-in button gating, shareAccount/shareRole/shareInvite disabled while signed out, presenceStrip resolves. Verify: dev.py build → dev.py ui → dev.py check; commit `feat: live share dialog with server picker`; no code comments.

## Plan
1. Read ShareDialog.qml, DocumentWorkspace.qml, PresenceStrip.qml, sharecoordinator.h/.cpp, sharesettings.h, ui_test.cpp share section.
2. Rewrite ShareDialog.qml: computed serverModel (presets + "Custom…"), derived currentIndex binding (no imperative writes → no loops), custom URL TextField + Connect, Loaders for sign-in/signed-in, always-instantiated sharing block gated on signedIn, owner share/stop row, status label, close.
3. Place PresenceStrip inside MindCanvas top-right, z 11, visible only with participants.
4. Extend shareLivesInDocumentToolbar with dialog-driving assertions; close dialog via qScopeGuard to avoid modal leakage on failure.
5. Build, run ui suite, fix, rerun; run fast check; commit; write progress + report.

## Implementation Summary
- ShareDialog.qml fully rewritten (see task-6-report for objectNames and binding strategy).
- DocumentWorkspace.qml: presenceStrip added to MindCanvas (participants: share.presence, visible when non-empty).
- C++ gap: ShareCoordinator had no `presets` property — added `Q_PROPERTY(QStringList presets READ presets CONSTANT)` delegating to m_settings->presets().
- ui_test.cpp: extended shareLivesInDocumentToolbar (server box count/model[0], sign-in enablement gating, disabled sharing controls, presenceStrip resolution, scope-guarded close).
- Verified: dev.py build clean; dev.py ui 100% passed (97.2 s); dev.py check 8/8 passed. One mid-task ui run failed on the date-dependent weeklyTemplatePicker "Monday" assertion (env flake, unrelated; passed on rerun).
- Skipped by design: "Shared with me"/SharedMaps.qml (no coordinator listing property yet).

## Next Steps
- Expose a joined/shared maps listing on ShareCoordinator and wire SharedMaps.qml.
- Enrich presence payloads (name+color objects) and adapt PresenceStrip rendering.
- Drive a full sign-in/share flow against a local share server in a dedicated integration test.
