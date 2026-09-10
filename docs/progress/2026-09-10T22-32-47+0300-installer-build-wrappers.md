# Initial Prompt
Create quick shell scripts to build macOS, Omarchy, and Windows installers and a build_all script.

# Plan
Reuse existing macOS and Windows release tooling; add native Arch packaging; support configurable remote build hosts; document prerequisites and verify orchestration without producing installers.

# Next Steps
Set VERSION and remote host/source/Windows Qt environment variables as documented in README, then run scripts/build_all.sh from macOS. Keep remote checkouts current before building.

# Implementation Summary
Added executable scripts/build_macos.sh, build_omarchy.sh, build_windows.sh, and build_all.sh. macOS delegates to the existing signed/unsigned Python release workflow. Windows delegates to the existing PowerShell/Inno Setup workflow locally in Git Bash or over SSH using safely encoded PowerShell. Omarchy builds a native makepkg package with Qt dependencies, launcher, MIME, thumbnailer, and icon in a temporary build directory. build_all attempts each platform sequentially and aggregates failures. Remote artifacts remain on their build hosts; no source synchronization or dependency installation is implicit.

Validation: Bash syntax passed for all four scripts. An isolated stub test verified success exit status and that a failed middle platform does not prevent the final platform from running. git diff --check passed. Actual installer builds were not run; no application restart was needed for this tooling-only change.
