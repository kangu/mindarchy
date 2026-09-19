# Mindarchy windows now tile like the terminal on Omarchy

## Initial Prompt
How do we make it so that on omarchy when i open a new window, the applications work well with the wayland tiling system like the terminal does for example, and creates smaller and smaller tiles on the same screen. Currently it opens full height windows always to the right.

## Plan
1. Trace the Hyprland window-placement code (windowplacement.cpp) and the new-window seeding path (macapplication.cpp) to find why new windows bypass the dwindle layout.
2. Fix the root causes: stop new Wayland windows from inheriting the last window's floating state, and make Hyprland client tracking survive document renames and sibling windows.
3. Rebuild and run the placement/application suites; document the refined behavior and the manual Omarchy verification.

## Next Steps
On the Omarchy machine: rebuild with the new packaging (`VERSION=0.1.5 bash scripts/build_omarchy.sh`) or the existing build, then verify manually — open a terminal, open Mindarchy (tiles and splits), Ctrl+N (the new window subdivides the focused tile, smaller and smaller), float a window and relaunch (its rectangle restores — feature), then Ctrl+N again (the new window tiles, not floats). Run `./scripts/test.sh` there so the native Hyprland windowplacement cases exercise the claim registry. Phase B/C of docs/omarchy-packaging-plan.md remain for later.

## Implementation Summary
Root cause was two stacked defects in the Hyprland placement code, not Hyprland itself:

1. **New windows inherited the last window's floating state.** `MacDocumentWindow::load()` seeds every new window's individual placement file from the global defaults, including `windowPlacement/v1`. Once any Mindarchy window had been saved while floating (Hyprland keeps the tile geometry as the float rectangle, so a floated right-half tile is a right-half full-height rect), every subsequently created window was force-floated to that exact rectangle via `setfloating` + `resizewindowpixel`/`movewindowpixel` dispatchers — never joining the tiling tree, hence "full height windows always to the right" while new terminals subdivide normally. Fixed in `macapplication.cpp`: the seed copy now skips `windowPlacement/v1` on Wayland, so a new window starts with no placement and Hyprland tiles it, splitting the focused tile; its own file then accumulates its own state. macOS and Windows keep the inherit-last-placement behavior.
2. **Geometry tracking froze after the window title changed.** `captureHypr` matched clients by `initialTitle == m_window->title()`, so once a document rename changed the title, the floating/rectangle capture stopped updating and the early map-time state — including a stale `floating: true` — persisted to save time forever. Fixed in `windowplacement.cpp/h`: each instance now claims its client's stable Hyprland address on first match (per-process registry, released on destruction) and tracks that address afterwards, so floating and geometry stay live for the window's whole life and sibling windows with identical titles never control each other.

Startup restore of a saved session is unchanged and intentional: the first window of a launch still restores its own saved placement (floating windows come back at their rectangle; tiled ones stay under Hyprland's control). With the fixes, tiled saves record `floating: false` reliably, so the stale-float loop also self-heals after one normal quit.

Verification: windowplacement and application suites pass on macOS (3/3, 29.3 s) after rebuilding the affected targets; the mindarchy target rebuilt. Cocoa behavior is unchanged (all changed paths are Wayland-gated), so no macOS restart was needed. The native Hyprland cases and the manual dwindle checks remain for the Omarchy machine; `docs/controls.md` documents the new no-inheritance rule.
