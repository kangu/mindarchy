# Omarchy packaging Phase A — checked-in PKGBUILD and native Omarchy tooling

## Initial Prompt
Proceed with phase A and let's leave the rest for later. Make sure the omarchy tooling is in place for me to start building the omarchy version from an omarchy machine rather than the current macos.

## Plan
1. Write a checked-in, omarchy-pkgs-ready `packaging/PKGBUILD` (literal pkgver 0.1.4, GitHub tag-archive source with pinned sha256, SPDX Apache-2.0 licence, full hicolor icon set installed in package()).
2. Polish `packaging/org.mindarchy.app.desktop` (GenericName, Keywords, StartupNotify).
3. Rewrite `scripts/build_omarchy.sh` to build the checked-in recipe with makepkg natively on the Omarchy machine: default mode packages the working tree via a mechanically derived PKGBUILD copy, `--release` runs the recipe verbatim; the macOS branch stays SSH-only delegation.
4. Verify locally (bash -n, derivation simulation, checksum) and record what still needs the Omarchy machine.

## Next Steps
On the Omarchy machine: pull this commit, install base-devel + qt6-base/qt6-declarative/qt6-wayland if missing, then run `VERSION=0.1.5 bash scripts/build_omarchy.sh` to package the working tree (or `VERSION=0.1.4 bash scripts/build_omarchy.sh --release` to rebuild the published tag). Then Phase C verification: namcap on PKGBUILD and package, launcher icons at several sizes, menu and terminal launch under Hyprland, portal file-picker, .omm association, thumbnailer, uninstall leftovers. Phase B (omarchy-pkgs fork and PR) stays for later.

## Implementation Summary
No macOS application changes, so no macOS rebuild or restart was needed.

- `packaging/PKGBUILD` (new, checked in): builds the GitHub tag archive (`v$pkgver`) with qmake6 PREFIX=/usr; `depends` qt6-base/qt6-declarative/qt6-wayland, `makedepends` gcc/make, `arch=('x86_64' 'aarch64')`, `license=('Apache-2.0')`, `options=('!debug')`; `package()` runs `make INSTALL_ROOT` and then installs all nine hicolor icon sizes as `org.mindarchy.app.png` via `install -Dm644` (qmake cannot rename during install; the 512px file qmake installs is identical content). The pinned sha256 `a3d3ba32d2487b99851619ffea99cec7092e93e5d7dbeb4f2eaec122261cbb07` was computed from the real v0.1.4 tag archive on GitHub.
- `packaging/org.mindarchy.app.desktop`: added `GenericName=Mind Map Editor`, `Keywords=mind map;mindmap;brainstorm;notes;tasks;planning;`, `StartupNotify=true`.
- `scripts/build_omarchy.sh` (rewritten): Linux-native makepkg flow. Default mode requires `VERSION=major.minor.patch`, stages the working tree into `mindarchy-$version.tar.gz` (top-level dir matches `$pkgname-$pkgver`), computes the sha256, and derives a temp PKGBUILD from the checked-in one by substituting only pkgver/source/sha256sums — build(), package(), deps and licence always come from the single recipe. `--release` copies `packaging/PKGBUILD` verbatim and requires the tag to be published. Output lands in `artifacts/omarchy/` with the pacman -U hint; the macOS branch still delegates through `OMARCHY_HOST`/`OMARCHY_SOURCE` over SSH.
- Verification performed here: `bash -n` on both files; a local simulation of the derivation produced `pkgver='0.1.5'`, `source=("mindarchy-0.1.5.tar.gz")` and the correct substituted checksum; the tag-archive sha256 was computed from the actual GitHub download. What still needs the Omarchy machine: the actual makepkg build, namcap, and the desktop-integration checks (Phase C).
- Plan doc updated: Phase A marked DONE with the icon-installation deviation noted.
