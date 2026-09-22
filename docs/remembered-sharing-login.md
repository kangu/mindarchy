# Remembered sharing login

Sign in once in the Share dialog. Mindarchy remembers the login in the operating system's credential vault and reconnects on subsequent launches. Sign out is visible near the top of the dialog, including when an offline startup has a saved login. Existing installations need one successful sign-in with this version before automatic reconnection is available.

The current server uses expiring CouchDB session cookies and has no refresh-token endpoint. To continue working after those cookies expire, the application stores the username and password in the device vault, scoped to the normalized sharing-server URL. It obtains a new server session at startup and renews it every five minutes while running. Credentials are never stored in map files, QSettings, command-line arguments, or a plaintext fallback file.

- macOS: Keychain generic-password item under `org.mindarchy.sharing`.
- Windows: user-scoped Windows Credential Manager generic credential, persistent on this computer.
- Linux/Omarchy: Secret Service via `secret-tool` from `libsecret`, with an active, unlocked keyring service. The Arch package depends on libsecret. If the vault is unavailable, the dialog reports that sign-in cannot be remembered; it does not silently write credentials to disk.

Temporary network/server failures retain the saved login and retry every 30 seconds. An explicit authentication rejection removes it and asks the user to sign in again. Unlock failures are reported so the user can unlock their vault and retry sign-in. Signing out deletes the saved login, clears local cookies and account state, cancels pending authentication requests and renewal timers, and signs out other tabs using that server in the same application process. Server-side session invalidation is not available through the current API; sign-out clears this device's saved credentials and live connections.

Normal tests inject an in-memory vault and use localhost HTTP fixtures. They cover recreated clients, server isolation, offline retries, invalid saved credentials, sign-out across tabs, and sign-out preventing restoration. Headless offscreen application runs do not access the user's vault.

An opt-in `ShareClientTest::deviceVaultSurvivesProcessRestart` check writes synthetic credentials under a unique test server identity, verifies them in a separate process, and removes them. Run with `MINDARCHY_TEST_DEVICE_VAULT=1` only when native vault access is intended. This test was verified on macOS; Windows Credential Manager and Linux Secret Service need platform runtime validation before release.
