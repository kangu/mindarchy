# Omarchy distribution guidance

## Initial Prompt
Explain how to distribute the Linux application for Omarchy.

## Plan
Inspect the existing native packaging wrapper and document build, sharing and installation commands.

## Next Steps
Build on an up-to-date Omarchy/Arch machine and distribute its architecture-specific package. No package was built in this task.

## Implementation Summary
Confirmed makepkg produces a native package in artifacts/omarchy, installed with pacman -U. Fixed the source staging list to include fonts.qrc, required by the newly bundled theme fonts. bash -n passed. This shell-only correction does not require a macOS application rebuild or restart.
