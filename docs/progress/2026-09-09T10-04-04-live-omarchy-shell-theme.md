# Initial Prompt
Adapt the application shell to the active Omarchy system theme and refresh live when it changes.

# Plan
1. Inspect Omarchy palette sources and theme switching.
2. Implement validated live palette loading and replacement-safe watches.
3. Bind shell surfaces, icons, and control palettes while preserving document styling.
4. Test live dark/light changes, replacement, malformed data, unchanged document state, and rebuild.

# Proposed Next Steps
Build and verify on the Omarchy machine once SSH connectivity is available.

# Implementation Summary
Added a Linux-only Omarchy palette adapter reading the active colors.toml. QFileSystemWatcher plus a one-second recovery timer handles edits, directory replacement, symlinks, and late availability. Validated palettes update shell colors through a QML singleton; interrupted or invalid updates retain the last valid theme. Toolbar icons now tint with shell foreground, and panels, native Qt control palette roles, dialogs, and tooltips adapt. Mindmap themes, preset colors, document bytes, and zoom remain unchanged. macOS build succeeded. All six CTest suites passed; the live-switch test passed again after final palette-role changes. Visually verified a light shell with the original dark document canvas. Direct SSH to omarchy-host timed out, so no remote deployment or on-machine test was performed.
