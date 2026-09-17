# Omarchy packaging best-practices analysis and compliance plan

## Initial Prompt
Analyse thoroughly step by step https://omarchyapps.com/develop and make a plan to put everything in place so the Omarchy best practices are being followed.

## Plan
1. Fetch and analyse the unofficial Omarchy packaging checklist page section by section.
2. Verify against the authoritative repo it references (omacom/omarchy-pkgs): layout, `bin/add-package`, `.omarchy/package.json` schema, channel pipeline.
3. Audit Mindarchy's Omarchy packaging state (build script, desktop file, icons, mime, releases) against every requirement.
4. Check for duplicates in AUR and omarchy-pkgs.
5. Write the compliance plan into docs/omarchy-packaging-plan.md.

## Next Steps
Execute the plan phases in order: Phase A app-side fixes (prototype.pro full icon set, desktop-file polish, consolidate build_omarchy.sh onto the checked-in PKGBUILD), Phase B fork omarchy-pkgs and scaffold `pkgbuilds/mindarchy/` with a source-build PKGBUILD and GitHub-releases upstream block plus 24h quarantine, Phase C namcap/chroot/launcher/portal/uninstall verification on the Omarchy machine, Phase D focused PR with the test matrix and disclosures, Phase E switch the website and README to `pacman -S mindarchy` once accepted.

## Implementation Summary
Analysis and plan only; no application code changed.

The develop page is an unofficial 34-point checklist (source choice, scaffolding, metadata, file safety, desktop integration, new-user testing, PR preparation) that defers to the official omarchy-pkgs repo. The official repo builds PKGBUILDs from local sources and AUR, signs them, and syncs to pkgs.omarchy.org through an edge → rc → stable channel pipeline; contributions add `pkgbuilds/<name>/PKGBUILD` plus `.omarchy/package.json` (source "local", one GitHub-releases upstream block, optional `min_release_age` quarantine, no fast ring unless asked) via PRs.

Mindarchy already satisfies: first-party Apache-2.0 upstream with homepage and tagged checksummed releases; no duplicate (0 AUR hits, absent from the repo's 144 packages); correct depends/makedepends split; standard file layout with no privileged actions and no user-data deletion on uninstall; valid desktop file whose StartupWMClass matches the Qt Wayland app_id; ~9 MB system-Qt package with no bundled runtimes.

Gaps: no checked-in omarchy-pkgs recipe (today's package comes from an ad-hoc generated PKGBUILD in build_omarchy.sh), no upstream tracking metadata, the qmake path installs only a 512px icon while CMake installs 16–1024, the desktop file lacks GenericName/Keywords/StartupNotify, and there is no namcap/chroot evidence or aarch64 story.

The full plan (Phases A–E with concrete steps, the source-build-vs-binary decision recommending a source build so the farm produces native x86_64 and aarch64 packages, and the min_release_age quarantine recommendation) is in docs/omarchy-packaging-plan.md.
