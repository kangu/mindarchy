# Share dialog scrolls within viewport with tighter spacing

## Initial Prompt
User reports the Share dialog Popup renders taller than the screen on a 1080p Wayland desktop (nebula), cutting off buttons and leaving a big blank gap between sections (inactive sign-in Loaders and generous fillWidth spacing). Fix qml/ShareDialog.qml to be compact and fit any screen: wrap the ColumnLayout in a Flickable with a vertical ScrollBar, cap height to the overlay viewport, tighten spacing 12 → 8, keep all objectNames/ids/signals intact, no QML comments, keep popup width 400/modal/centered.

## Plan
1. Wrap the contentItem ColumnLayout into a Flickable (`shareFlick`) with `contentHeight: shareColumn.implicitHeight`, `interactive` only when overflowing, and `implicitHeight` capped at `Overlay.overlay.height - 120`.
2. Attach a non-interactive `ScrollBar.vertical` inside the Flickable.
3. Reduce top-level spacing from 12 to 8; leave inner section spacings and all objectNames/ids unchanged.
4. Keep Loaders width-only fill (no fillHeight) so they collapse when inactive.
5. Verify: `dev.py build`, `dev.py check`, `dev.py ui`; commit; add progress note.

## Implementation Summary
- qml/ShareDialog.qml: contentItem is now a Flickable containing the original ColumnLayout (id `shareColumn`, width bound to `shareFlick.width`). Top-level spacing tightened 12 → 8. Vertical ScrollBar added (non-interactive). Flickable height capped to `Overlay.overlay.height - 120` when an overlay exists. No QML comments added. All objectNames, control ids, bindings, and signals unchanged: shareDialog, shareStatusLabel, shareServerBox, shareConnect, shareSignInAccount/Password, shareSignIn, shareSignOut, shareAccount, shareRole, shareInvite, shareJoinToken, shareJoin, shareRefreshMaps, shareOpenSharedMaps, shareShare, shareStopSharing, shareClose. Popup width stays 400, modal, centered (parent-set anchors untouched).
- Build: `dev.py build` completed successfully.
- `dev.py check`: all 22 tests passed (fast + offscreen, 22.74s each leg).
- `dev.py ui`: 100% tests passed, 0 failed out of 1 (96.86s). No flake observed on this run.
- Commit: `fix: share dialog scrolls within viewport with tighter spacing` (605e506).
- No app restart needed (build-only change).

## Next Steps
- Visually confirm on a 1080p Wayland desktop (nebula) that the dialog fits and scrolls; adjust the 120px viewport margin if the top anchor offset needs tuning.
- Consider a similar Flickable wrapper for other tall popups (e.g. SharedMaps popup) if they overflow on small screens.
