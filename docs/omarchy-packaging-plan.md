# Omarchy packaging compliance plan

Source of requirements: https://omarchyapps.com/develop (unofficial checklist, reviewed
2026-09-17) cross-checked against the authoritative repository it points to:
https://github.com/omacom/omarchy-pkgs. Date: 2026-09-18.

## 1. What the Omarchy process requires (step-by-step analysis)

The official `omarchy-pkgs` repository is the build system for the Omarchy Package
Repository: it builds PKGBUILDs from local sources and AUR, signs them, and syncs to
`pkgs.omarchy.org` through a forward-only edge → rc → stable channel pipeline driven by
systemd timers. Contributions are PRs that add one directory per package.

| Step | Requirement |
| --- | --- |
| 1. Source | Stable upstream release, real homepage, redistribution-permitting licence; no duplicates in the official repo or AUR; prefer existing AUR recipes, local packaging only when genuinely needed; verify every release URL, asset name, architecture, version tag, checksum. |
| 2. Scaffold | Work in a fork of `omacom/omarchy-pkgs`; each package in `pkgbuilds/<name>/` with `PKGBUILD` and `.omarchy/package.json`; new local packages via `bin/add-package <name> --scaffold`; lowercase Arch-convention names; minimal metadata; lasting customizations as patches applied in `prepare()`. |
| 3. Metadata | Accurate pkgver/pkgrel/description/arch/URL/licence; runtime deps in `depends`, build tools in `makedepends`; `provides`/`conflicts`/`replaces` only with exact semantics; real checksums; per-arch asset mapping or explicit `arch` limits. |
| 4. Files | Standard locations (bundled apps under `/opt/<app>` with launchers on PATH, or normal system paths for regular binaries); non-executable data files; no setuid/elevated post-install; no hardcoded usernames/paths/secrets; uninstall removes package files without touching user data. |
| 5. Desktop integration | Valid `.desktop` launcher (matching Exec/Icon/categories/startup); icons in standard hicolor locations and visible in the Omarchy launcher; launch from terminal and menu under Hyprland; watch Wayland/portal/tray/notification/file-picker behavior. |
| 6. Testing | Clean checkout/chroot build; `namcap` on PKGBUILD and package; repo dry-run; install → exercise as a normal user → re-login if session integration matters → uninstall; inspect file list, ownership, permissions, size, absence of build-machine paths; keep downloads small (no bundled toolchains, strip debug); test each declared arch or disclose. |
| 7. PR | Focused diff; describe the app, why it belongs, source origin, update tracking, exactly what was tested; disclose limitations (arch gaps, permissions, network services); expect maintainer revisions. |

Useful `.omarchy/package.json` fields (official README): `source: "local"`, one mutually
exclusive `upstream` block (GitHub releases / git tags / npm / debian) or an `upstream.sh`
hook, `release_ring` (avoid `"fast"` unless asked), `channels`, `min_release_age`
(quarantine, e.g. `"24h"`), `sync: false` for maintenance holds. Version rules: pacman
vercmp ordering, attached pre-release forms (`0.2.0rc1`, not `0.2.0-rc1`), pkgrel resets to 1
per version, epoch never set by tooling.

## 2. Mindarchy state vs. the requirements

### Already compliant

- **Source/licence**: first-party upstream at github.com/kangu/mindarchy, homepage
  mindarchy.xyz, Apache-2.0. **Not a duplicate**: 0 AUR results, absent from the 144
  packages in omarchy-pkgs.
- **Metadata discipline**: PKGBUILD deps (`qt6-base`, `qt6-declarative`, `qt6-wayland`)
  vs makedepends (`gcc`, `make`) correctly split today; ~9 MB package linking system Qt —
  no bundled runtimes, no toolchain downloads.
- **File layout**: `/usr/bin/mindarchy` plus `/usr/share/{applications,mime,thumbnailers}`
  and hicolor icons; no setuid, no post-install scripts, no elevated actions, no secrets or
  local paths; uninstall never touches QSettings or user `.omm` documents.
- **Desktop integration**: `org.mindarchy.app.desktop` with `Exec=mindarchy %f`,
  `MimeType=application/x-omm+json`, thumbnailer, and `StartupWMClass=org.mindarchy.app`
  which matches the Qt Wayland app_id (`setDesktopFileName("org.mindarchy.app")` in
  `main.cpp:80`).
- **Release hygiene**: tagged GitHub releases with SHA256SUMS; semver versions; Windows-style
  placement/portal behavior already exercised on Hyprland by the window-placement suite.

### Gaps

1. **No omarchy-pkgs recipe exists** — today's package is produced by an ad-hoc PKGBUILD
   generated inside `scripts/build_omarchy.sh`; nothing is checked in for the official repo.
2. **No `.omarchy/package.json` upstream tracking** — releases are manual.
3. **Icon gap in the qmake path**: `prototype.pro` installs only the 512px icon, while CMake
   installs 16–1024. The Omarchy build (qmake) ships a launcher icon that scales poorly.
4. **Desktop file polish**: no `GenericName`, `Keywords`, or `StartupNotify=true`.
5. **No namcap/chroot evidence** and no aarch64 story (PKGBUILD declares it; only x86_64 is
   ever built).
6. **Two divergent packaging paths** (generated qmake PKGBUILD vs a future checked-in one).

## 3. Plan

### Phase A — app-side fixes (qt-prototype) — DONE 2026-09-18

1. ~~`prototype.pro`: install the full hicolor set~~ → implemented in the checked-in
   `packaging/PKGBUILD` instead: qmake cannot rename files during `make install`, so the
   PKGBUILD's `package()` installs all nine sizes as `org.mindarchy.app.png` with
   `install -Dm644` (the 512px path qmake already covers is identical content and is simply
   overwritten). CMake keeps covering the non-PKGBUILD install path.
2. `packaging/org.mindarchy.app.desktop`: added `GenericName=Mind Map Editor`,
   `Keywords=mind map;mindmap;brainstorm;notes;tasks;planning;`, `StartupNotify=true`.
   DONE.
3. Licence identifier: `license=('Apache-2.0')` (SPDX form) in the checked-in PKGBUILD;
   confirm with namcap in Phase C.
4. Consolidated: `scripts/build_omarchy.sh` now builds the **checked-in PKGBUILD** via
   makepkg. On the Omarchy machine: default mode packages the current working tree (stages
   the archive, mechanically substitutes pkgver/source/sha256 into a copy of the recipe);
   `--release` runs the recipe verbatim against the published GitHub tag. The macOS branch
   only delegates over SSH.

### Phase B — omarchy-pkgs recipe (fork)

5. Fork `omacom/omarchy-pkgs`; create a branch; `bin/add-package mindarchy --scaffold`.
6. Write `pkgbuilds/mindarchy/PKGBUILD`: source-build from the GitHub tag archive
   (`https://github.com/kangu/mindarchy/archive/refs/tags/v$pkgver.tar.gz`), qmake6 build of
   `prototype.pro` with `PREFIX=/usr`, `depends`/`makedepends` as today,
   `arch=('x86_64' 'aarch64')` — the farm builds natively per arch, which gives aarch64
   support without new release assets. (Alternative rejected: a -bin package would need a
   new binary asset pipeline and loses the farm's per-arch builds.)
7. `pkgbuilds/mindarchy/.omarchy/package.json`: `{"source":"local"}` + a GitHub-releases
   upstream block for `kangu/mindarchy` so `bin/sync-upstream` bumps pkgver and checksums
   from vendor manifests per release; `min_release_age: "24h"` (quarantine tip); **no**
   `release_ring: "fast"`; no channels restriction.
8. No patches needed (`prepare()` stays stock); document that uninstall never removes user
   data (PR description + a PKGBUILD comment).

### Phase C — verify like a new user (Omarchy machine)

9. `namcap` on the PKGBUILD and the built package; resolve warnings, don't dismiss them.
10. Clean chroot build (devtools) and/or the repo's dry-run to inspect the build plan.
11. Clean install: icon visible in the Omarchy launcher at several sizes; launch from the
    application menu **and** a terminal under Hyprland; open/save `.omm` through the file
    picker (portal check); double-click association; thumbnailer on a sample map; re-login
    once; uninstall; confirm `pacman -Qo` file list, permissions, size, and that
    `~/.config`/user documents survive.
12. aarch64: test on hardware if available, otherwise disclose as unverified in the PR.

### Phase D — submit

13. Focused PR containing only `pkgbuilds/mindarchy/`; description covers: what Mindarchy
    is, why it belongs in Omarchy (Omarchy-first mind mapping, live shell-theme
    integration), source origin and licence, update tracking (GitHub releases upstream
    block), the exact test matrix from Phase C, and disclosures (aarch64 unverified on
    hardware if applicable; Qt portal file-picker caveats).
14. Iterate on maintainer revisions; after merge, releases flow edge → rc → stable via the
    farm; future releases only need a tagged GitHub release (automation updates the recipe).

### Phase E — follow-through

15. Once live in the Omarchy repo, update the website `/install/omarchy` page and README to
    offer `pacman -S mindarchy` (or the Omarchy app store entry) as the primary method, with
    the manual `pacman -U` flow as fallback for preview builds.

## Decision points

- **Source build vs binary package**: source build recommended (farm-native per-arch builds,
  tag-archive checksums, no new assets); revisit only if the maintainers prefer binaries.
- **`min_release_age`**: "24h" recommended; it delays fresh releases by a day by design.
- **aarch64 claim**: declare it only if the farm build passes and someone smoke-tests on ARM
  hardware; otherwise limit `arch` to x86_64 and disclose.
- **Pre-releases**: if ever published to the repo, use attached forms (`0.2.0rc1`).
