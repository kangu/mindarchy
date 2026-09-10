# macOS installer releases

The release command builds the Qt prototype and creates a self-contained `.pkg` installer for **/Applications/Mindarchy.app**. Recipients do not need Qt installed. The command runs on a macOS desktop session with Python 3.9+, CMake, Xcode command-line tools, and a Qt 6 kit including Qt Test and `macdeployqt`.

## Build a local test installer

From `qt-prototype`:

```bash
python3 scripts/release-macos.py --version 0.1.0 --unsigned
```

The default Qt kit is `~/Qt/6.11.2/macos`; override with `--qt /path/to/Qt/macos` or `QT_PREFIX_PATH`. Each run writes a separate timestamped directory under `dist/macos/<version>/<arch>/` and never replaces an earlier release. Development builds are unchanged; release compilation uses `build-release-macos/<arch>/`.

Outputs:

- `Mindarchy-<version>-macos-<arch>-unsigned.pkg` (or the signed filename without `-unsigned`).
- `SHA256SUMS` and `release.json`: artifact checksum, version, architecture, Qt version and signing/notarization state.
- `Mindarchy.app`: the verified portable application extracted from the installer.
- `payload-smoke.png` and `release.log`: packaged-interface evidence and build/test/deployment logs.

The command only reports success after all tests pass, bundle linkage/signature checks pass, the installer can be expanded, and its actual application payload opens the full QML interface with Qt/DYLD environment overrides removed. It does not invoke the system installer or modify `/Applications`.

Unsigned mode uses ad-hoc app signatures and produces an unsigned installer for local testing. It is not a notarized distribution release; Gatekeeper may refuse an unsigned downloaded installer. Use the signed/notarized mode for distribution.

## Install or update

Close an installed copy of Mindarchy, then open the `.pkg` in Finder and follow macOS Installer. macOS may request administrator authorization. For managed deployment, an administrator can run:

```bash
sudo installer -pkg "/path/to/Mindarchy-0.1.0-macos-arm64.pkg" -target /
open "/Applications/Mindarchy.app"
```

The package uses receipt ID `org.mindarchy.app.installer` and stable app bundle ID `org.mindarchy.app`. Relocation is disabled, so the installer cannot accidentally update a development bundle elsewhere. Bundle version checking is enabled; release a higher numeric `major.minor.patch` for an upgrade. Upgrades replace the installed application bundle, including obsolete bundled libraries. There are no custom privileged installer scripts, launch agents, automatic app termination or automatic launch. User documents and settings are not included in the package and are not removed.

This is an installer deployment workflow, not an in-app auto-updater or a hosting service. Distribute a new installer to deploy each version. Installer upgrades and downgrade behavior still need a clean-machine administrative install test; the automated check verifies the expanded payload without changing the host installation.

## Signed and notarized distribution

Install both Apple Developer ID certificates and their private keys in the build user's Keychain:

- **Developer ID Application** signs the app and bundled code using hardened runtime and timestamps.
- **Developer ID Installer** signs the `.pkg`.

Store notarization credentials once using Apple's interactive Keychain setup (credentials are not written into this repository):

```bash
xcrun notarytool store-credentials "mindmap-notary"
```

Then release:

```bash
export MACOS_APP_SIGN_IDENTITY='Developer ID Application: Your Name (TEAMID)'
export MACOS_INSTALLER_SIGN_IDENTITY='Developer ID Installer: Your Name (TEAMID)'
export MACOS_NOTARY_PROFILE='mindmap-notary'
python3 scripts/release-macos.py --version 0.1.1 --sign --notarize
```

The workflow excludes optional ODBC, PostgreSQL and Mimer drivers and retains SQLite, preventing accidental dependencies on local database client installations. It re-seals the outer app signature after pruning plugins. The workflow uses `macdeployqt` to sign nested Qt code and the app; a JIT entitlement supports QML execution. It signs the package, submits it with `notarytool --wait`, requires an Accepted response, staples and validates the ticket, and checks Gatekeeper's installer assessment before writing the final checksum. A failed submission stops the release and retains logs; no artifact is uploaded to a public hosting destination.

`--sign` without `--notarize` is available for testing a signed installer, but the manifest explicitly records `notarized: false`. This Mac currently has no valid signing identities, so only the unsigned path has been exercised end-to-end. Signed/notarized operation must be validated after credentials are installed.

## Architectures and CI

The default architecture is the current host. Use `--arch arm64`, `--arch x86_64`, or `--arch universal`. The Qt kit must contain every requested architecture; the script checks every deployed Mach-O file as well. Only the native architecture is exercised by the smoke test. Intel/universal artifacts and older macOS versions need testing on their target hardware.

`--min-macos 13.0` is the default deployment target, not proof of testing on macOS 13. The verified machine runs macOS 15.7.4. Build and Qt deployment targets must remain compatible.

The same release command can be called by a macOS CI worker with a GUI session, Qt, the Apple toolchain and an unlocked signing Keychain. The Qt prototype has no configured repository hosting/CI provider, so this workflow does not invent a GitHub repository or publish to one. Archive the timestamped release directory when the command exits zero; only publish manifests marked signed and notarized for public distribution. Never put private keys or notarization passwords into source files or logs.

References: [Qt macOS deployment](https://doc.qt.io/qt-6/macos-deployment.html), [Apple Developer ID](https://developer.apple.com/developer-id/), [Apple notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution).


## Verified artifact — 8 September 2026

Built an unsigned arm64 0.1.0 installer (54 MiB) on macOS 15.7.4 with Qt 6.11.2. All four release CTest suites passed. Both staged and extracted payloads passed signature/linkage/architecture checks across 98 Mach-O files. The extracted app reported version 0.1.0, opened the full native QML interface and produced a visually checked screenshot. The package checksum, fixed destination, non-relocation, strict bundle ID, version checking and upgrade metadata were verified.

Artifact: `dist/macos/0.1.0/arm64/20260908T064237110951Z/`. No administrative installation or clean-machine upgrade was performed. Signed/notarized and Intel/universal releases remain untested.

Qt’s deployment log may mention missing client libraries for optional SQL drivers before they are pruned. The final bundle verifier rejects any remaining non-system external dependency; the verified artifact contains only the SQLite SQL driver.

The workflow also produces a verified DMG containing the same app plus an Applications shortcut. Both delivery formats bundle the sandboxed OMM Quick Look preview extension and its offscreen Qt plugin. Signed/notarized mode signs and notarizes both distribution artifacts.
