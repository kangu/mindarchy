# Share icon beside File and Add

## Initial Prompt
Move the Share button next to the File and Add dropdowns, use icon only.

## Plan followed
Move Share into the document action row after Add, add a bundled share SVG, retain the tooltip and accessible name, build and verify.

## Next Steps
No further work needed for this layout change. Online collaboration remains a separate outstanding feature.

## Implementation Summary
Share now follows Add as an icon-only toolbar action. Removed its old right-edge space reservation. Added share.svg to Qt resources. Build and all four fast suites passed; targeted share dialog and toolbar layout tests passed. Restarted once and visually verified the new placement with the user's recovered map reopened. Existing font-alias warning remains.
