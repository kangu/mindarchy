# macOS close and minimize buttons

## Initial Prompt
On macOS, the top-left minimize and close buttons do not function properly.

## Plan
1. Stop the header drag MouseArea from covering the traffic-light region.
2. Forward mouse hits in those button rects to the real NSButtons, because full-size content sits above the titlebar.

## Next Steps
Manually click close, minimize, and zoom (including Option-zoom). Cmd-W close is unchanged.

## Implementation Summary
The header drag area now starts 96px from the left on macOS, matching the toolbar inset, so it no longer starts a window move on the lights. A local AppKit event monitor sends press/drag/move in that cluster to the standard close, miniaturize, and zoom buttons.

macapplication and macwindow tests passed. Rebuilt and restarted the macOS app. Full suite skipped. Shortcuts unchanged.
