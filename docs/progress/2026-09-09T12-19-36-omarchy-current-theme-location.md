# Initial Prompt
Omarchy Ristretto theme does not update the application header.

# Plan
Inspect live installed theme paths, fix discovery with legacy compatibility, update the open process safely, test live palette changes and deploy.

# Proposed Next Steps
Continue manual testing; subsequent app launches use the updated binary directly.

# Implementation Summary
Diagnosed on the connected Omarchy 4.0.2 machine: theme state moved from ~/.config/omarchy/current to ~/.local/state/omarchy/current. Updated ShellTheme to prefer XDG state storage, retain legacy config fallback, watch both locations, preserve the last valid palette during atomic switches, and use the current red key as well as legacy color1. Added a compatibility symlink /home/user/.config/omarchy/current -> /home/user/.local/state/omarchy/current so the already-running PID 112356 updated without restarting or touching unsaved documents. Visually verified its brown Ristretto header and later its light Rose Pine header after the user switched themes. Built and deployed the permanent fix on Omarchy; rebuilt macOS. Both live theme tests passed on macOS and Omarchy, including migration, precedence and replace-gap handling. A standalone probe using the deployed Linux objects detected #faf4ed, matching the active Rose Pine palette. Updated the qmake UI test manifest and README. No packages built.
