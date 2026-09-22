# Apple distribution signing investigation

## Initial Prompt
I have renewed my Apple membership program; investigate issuing the needed certificates and profiles and signing the built artefact.

## Plan
Inspect release artifacts and entitlements, check local signing identities and notarization setup, inspect the renewed team in Xcode, and identify issuance requirements before signing a preserved copy of the release.

## Proposed Next Steps
Sign in to the Apple Developer portal with the renewed membership's Account Holder. Confirm membership/team and any pending agreements. Issue or import Developer ID Application and Developer ID Installer certificates with matching private keys. Configure notarization credentials in Keychain. Re-sign a copy of the existing 0.1.6 app and all nested code, rebuild the delivery containers, notarize, staple, assess Gatekeeper, and regenerate checksums. Do not rebuild unrelated working-tree changes or overwrite existing release artifacts.

## Implementation Summary
- Latest packaged release: dist/macos/0.1.6/arm64/20260922T063009530719Z, bundle org.mindarchy.app, arm64, 102 Mach-O files per manifest. Existing PKG and DMG are unsigned and not notarized.
- Local Keychain has one valid identity: Apple Development: apple@kangu.ro (2PU4NS48AQ). No usable Developer ID Application or Developer ID Installer identity is present.
- Xcode account apple@kangu.ro shows KANGU STUDIO S.R.L. with Admin role, plus Radu Stanciu (Personal Team). The company certificate manager shows an expired Mac Installer Distribution certificate. All certificate creation menu items, including Developer ID Application and Developer ID Installer, are disabled.
- Apple documents Account Holder role for creating local Developer ID certificates. Renewal activation cannot be confirmed from the observed Xcode UI; no claim that renewal failed.
- Current app entitlement is allow-jit; Quick Look extension entitlements are app-sandbox and user-selected read-only files. No restricted capability requiring a Developer ID provisioning profile was found in these entitlement files.
- Expected notarization Keychain profile mindmap-notary is absent, confirmed by notarytool history. No secrets were retrieved.
- Opened developer.apple.com/account in the Codex browser; it requires sign-in. Left the tab available for user authentication.
- No certificates were issued or revoked, and no release artifacts were changed, signed, uploaded, or published.

Reference: https://developer.apple.com/help/account/certificates/create-developer-id-certificates

## Signed-in account follow-up
- Portal confirms Account Holder Radu Stanciu, KANGU STUDIO S.R.L., team UYFX7NN4R7. Renewal date September 23, 2027. Current program agreement accepted September 22, 2026. Earlier Xcode Admin status was stale.
- Portal certificate list is empty; both Developer ID certificate types are enabled.
- Generated separate RSA 2048-bit private keys and SHA-256 CSRs for Application and Installer in ~/Library/Application Support/Mindarchy/signing/UYFX7NN4R7-20260922. Directory permission 0700; keys 0600; both CSR self-signatures verified. Keys are outside the repository and have not been uploaded.
- Selected G2 intermediary and staged the Application CSR on Apple's form. Continue is enabled but has not been clicked to issue the certificate.
- Browser computer-use policy requires action-time confirmation for creating security-sensitive credentials. Waiting for confirmation to issue both Developer ID certificates; artifact signing and notarization remain pending.
