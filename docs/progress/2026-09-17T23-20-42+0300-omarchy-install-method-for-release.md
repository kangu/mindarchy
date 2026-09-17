# Omarchy install method for the website release section

## Initial Prompt
What would be the installation method for Omarchy to present at https://mindarchy.xyz/#get? What is the easiest and cleanest install on Omarchy, and what binaries must the GitHub release ship to make it work?

## Plan
1. Confirm what the Omarchy package contains and declares (scripts/build_omarchy.sh PKGBUILD, prototype.pro install targets).
2. Confirm how scripts/release-github.py stages Omarchy assets.
3. Check the live mindarchy.xyz #get and /install/omarchy placeholders.
4. Recommend the install presentation and the asset list; no code changes.

## Next Steps
1. Finish the 0.1.4 release (Windows/macOS rebuilds, then `release-github.py --version 0.1.4`).
2. Fill the website #get Omarchy card and /install/omarchy with the pacman one-liner, requirements, SHA256, upgrade and uninstall notes.
3. When the release cadence stabilises, add an AUR `mindarchy-bin` PKGBUILD that downloads and hash-verifies the release asset; consider a hosted pacman repository only after that.

## Implementation Summary
Analysis only; no application code changed and no rebuild performed.

Recommendation: present the native pacman package — download the release asset and install with `sudo pacman -U mindarchy-<version>-1-x86_64.pkg.tar.zst`. It is the easiest and cleanest path because the package is a first-class pacman unit: `depends=('qt6-base' 'qt6-declarative' 'qt6-wayland')` resolves automatically from the Arch/Omarchy repos at install time (the package is ~8.7 MB because it links system Qt, so nothing is bundled), and the qmake install targets place `/usr/bin/mindarchy`, the `.desktop` entry, the `.omm` MIME XML, the thumbnailer, and the hicolor icon — pacman's standard hooks register associations and icons without post-install steps. Uninstall and upgrade are plain pacman operations; the package is foreign (not from a repo), so upgrades stay manual until an AUR package or hosted repo exists.

GitHub release assets needed: exactly one file per architecture — the already-staged `artifacts/omarchy/mindarchy-0.1.4-1-x86_64.pkg.tar.zst` plus the publisher-generated SHA256SUMS (release-github.py already accepts this name pattern and optional `.sig`). No AppImage, tarball, or bundled-Qt build is required. The asset filename embeds the version, so a `releases/latest/download/` one-liner must be updated (or generated) per release; aarch64 is declared in the PKGBUILD but not built yet; no `.sig` is produced today.
