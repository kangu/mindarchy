# Development-signed macOS build

## Initial Prompt
Use the signing certificate added in Xcode to produce a signed macOS build.

## Plan
Inspect available identities, run release checks, deploy Qt dependencies into a separate bundle, sign nested code and the app, verify the bundle and launch it, then package and verify the archive.

## Proposed Next Steps
For public downloads with normal Gatekeeper acceptance, add Developer ID Application and notarize. A signed PKG also requires Developer ID Installer. The installed Apple Development identity cannot replace these distribution certificates.

## Implementation Summary
- Found Apple Development: apple@kangu.ro (2PU4NS48AQ), identity BBFBD6B0BA1AFA49B9CDC21757689D1CE7167114. No Developer ID Application identity was available.
- Built current working-tree changes as Mindarchy 0.1.0 for arm64 and packaged a standalone app with Qt and Quick Look dependencies.
- Signed using the available certificate, secure timestamps, hardened runtime, and existing app/extension entitlements.
- Release checks passed: 12/12 offscreen suites, 4/4 native suites. Bundle metadata, architecture, bundled dependencies and deep strict signature verification passed. The deployed app launched and rendered the sample map offscreen. Extracted ZIP signature verified again.
- Output: artifacts/development-signed/20260922-082810/Mindarchy-0.1.0-macos-arm64-development-signed.zip. Directory also contains the signed app, SHA256SUMS, signing.json, smoke image, and build/test logs.
- This is explicitly a development-signed, non-notarized build. No installer signature, notarization, deployment or public release is claimed. Existing working app was not replaced or restarted.
