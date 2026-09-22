# Remember sharing login on this device

## Initial Prompt
Keep sharing login across application restarts, start online automatically, and provide a sign-out option.

## Plan
1. Trace login lifetime and server-session expiry.
2. Store remembered login only in native credential vaults, restore automatically, renew expiring sessions, and retry temporary failures.
3. Make sign-out prominent and remove saved credentials across tabs.
4. Verify recreated clients, native vault persistence in a separate process, UI behavior, and the quiet regression gate.

## Proposed Next Steps
Restart the application when convenient and sign in once with this build to create the saved device login. The normal quit request returned User cancelled, so the running session was preserved. Validate Windows Credential Manager and Linux Secret Service on their native platforms before release.

## Implementation Summary
- Reproduced loss of authentication in a new ShareClient instance with a failing regression test.
- Added a credential-store abstraction with macOS Keychain, Windows Credential Manager, and Linux Secret Service implementations. Linux uses secret-tool/libsecret; the Arch package now depends on libsecret. There is no plaintext fallback.
- The current backend issues short-lived CouchDB cookies without refresh tokens. Username/password are therefore stored inside the OS credential vault, scoped to the sharing server, and used to obtain new sessions at startup and every five minutes. Nothing is stored in map files or ordinary settings.
- Startup automatically restores saved login and joins an attached shared map through the existing authenticated join path. Session renewal updates transport cookies and reconnects without reloading the document snapshot.
- Temporary failures keep saved login and retry every 30 seconds; explicit authentication rejection removes it and asks for a fresh sign-in.
- Sign out is directly visible in the dialog, including offline saved-login state. It clears the saved login, cookies, pending requests, renewal timers, invitation state, presence, and active collaboration sessions across tabs for that server in this application process.
- Headless offscreen runs do not access the user's vault. Ordinary tests use a memory vault and localhost fixtures.
- macOS build passed. ShareClient tests passed 15/15 including an opt-in real Keychain test with synthetic credentials read in a separate process and deleted afterward. Focused UI tests passed 4/4 including setup/cleanup. ShareCoordinator tests passed 16/16, including a rerun after final sign-out cleanup. dev.py check passed all 11 suites. git diff --check passed.
- Windows/Linux native vault runtime verification remains for those platforms. No production credentials were inspected, and no production login or invitation was performed by tests. No commit, package, or deployment performed.
