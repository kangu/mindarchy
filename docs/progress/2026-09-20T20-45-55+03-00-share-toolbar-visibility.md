# Share toolbar visibility fix

## Initial Prompt
Proceed with the recommended fix for the missing top-right Share button.

## Plan followed
Trace native workspace ownership; move Share into the document toolbar with reserved layout space; prevent the placeholder invitation form from silently succeeding; run build, fast tests, targeted UI tests and native verification.

## Next Steps
Implement the planned authenticated desktop collaboration client (account login, map identity/upload, transport and invitation API) before enabling invitations. Existing collaboration session classes only provide offline storage; wiring the QML signal alone cannot send invitations.

## Implementation Summary
Moved Share from Main.qml's ApplicationWindow content container into DocumentWorkspace's toolbar, avoiding occlusion by the native-managed workspace. Reserved space outside the horizontally scrolling toolbar and platform controls. Centered the per-document dialog on the window overlay, committed pending editor text before opening, and included it in modal tab-switch protection. Replaced the nonfunctional invitation submission with an explicit unavailable explanation and disabled controls. Superseded the pre-existing z:99 workaround through relocation; preserved the unrelated CMakeLists.txt edit.

Validation: python3 scripts/dev.py build and check passed (4 fast suites). Targeted shareLivesInDocumentToolbar failed before the fix, passed after it. toolbarGroupsAndNewDocument passed. Offscreen Qt emitted its existing missing Sans Serif font-alias warning. Rebuilt app restarted gracefully once; reopened My meetings - recovered and verified the visible top-right Share button opens the centered dialog. Closed the dialog and left the user's map open. No invitations sent; online sharing remains unimplemented.
