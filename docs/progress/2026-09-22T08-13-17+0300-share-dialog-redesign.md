# Approachable sharing dialog

## Initial Prompt
Refactor and redesign the Share dialog and functionality to make it friendlier and more approachable.

## Plan
- Inspect the actual sharing, authentication, invitation, and disconnect behavior.
- Separate this-map sharing from shared-map browsing; move connection details into an expandable section.
- Provide clear permission labels, progress, outcome feedback, and a complete invitation-code flow.
- Verify with local/offscreen UI and service tests, build the application, and preserve the running user session.

## Proposed Next Steps
- Restart the running app when convenient to load the rebuilt version; normal quit was cancelled and no process was forced closed.
- Validate the collaboration flow against the intended deployment server before release. No production invitation or upload was performed during this task.

## Implementation Summary
- Replaced the tall combined form with a responsive 520px dialog, This map / Shared with me tabs, inline shared-map browsing, expandable connection settings, and a persistent Done/status footer.
- Added theme-aware colors, a short fade-in, field accessibility names, standard keyboard control behavior, and automatic scrolling of focused controls into view.
- Explained view/edit permissions and hid invitations from non-owners. Fields retain their contents after errors and clear only after server confirmation; pending submissions are disabled.
- Fixed the missing invitation-code flow: the client now retains the server-returned token, the dialog displays it with Copy code, and explains its account restriction and seven-day expiry. The application does not automatically message the collaborator.
- Renamed Stop sharing to Disconnect this map and explained that existing collaborators retain access.
- Implemented local sign-out: abort/disconnect pending replies, clear cookies and cached account/maps, and notify the UI. Changing servers signs out and detaches the previous map connection.
- Application and focused test targets built successfully. Two focused UI cases passed (4 including setup/cleanup), shareclient passed (10 including setup/cleanup), sharecoordinator passed (16 including setup/cleanup), and dev.py check passed all 11 suites. Rendered signed-in/out states inspected. Only the existing Sans Serif font-alias warning appeared in UI checks; no QML warnings.
- Normal application quit returned User cancelled; the running session was preserved. Updated binary is available in build-macos/mindarchy.app. No commit, packaging, or deployment performed.
