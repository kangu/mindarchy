# Links and Resources

## Initial Prompt

Lets work on Links and Resources next.

## Plan

Add named web and local file references to individual nodes, expose their management in the Node inspector, preserve relative references through document operations, and verify on macOS.

## Next Steps

Manual testing in the rebuilt macOS application. References to specific node IDs and embedded attachments remain future additions; ordinary linked .omm files can be opened with the system file association.

## Implementation Summary

- Added Links & Resources above the Node inspector's style controls. Each node supports multiple named web links and local file references, with Add link, Add file, Browse, Open, Edit, and Remove controls. Resource text is displayed as plain text; long paths are elided with hover tooltips.
- Added optional per-node JSON resources while keeping legacy records without resources unchanged. File targets are absolute at runtime and relative to the saved map when possible. Save As rebases serialization; clipboard interchange carries resolved targets. Resources participate in undo/redo and recovery.
- HTTP(S) links open only on explicit action through QDesktopServices. File references open through the system association. Missing targets show an error and can be edited to a new location. Files remain external, not embedded in the document.
- Added validation for resource types, URL schemes, lengths, and a 100-resource per-node limit. Malformed resource records are rejected by document loading. Unsaved documents require absolute file paths.
- Added engine coverage for relative paths, Save As, moving a map with adjacent resources, branch paste across document folders, recovery, undo/redo, missing files, and invalid URLs. URL dispatch is verified using intercepted Qt URL handlers, without launching external resources during tests.
- Added macOS UI coverage for Add link, Edit, Add file, Remove, and undo. Visually verified the themed inspector cards in artifacts/resources/resources-macos.png.
- Full CTest run passed all six suites. Final focused verification and restart are recorded below.
- Final engine and UI suites passed after the compatibility and dialog Undo adjustments (2/2 suites, 47.60 seconds). The preceding full run passed 6/6 suites in 54.77 seconds. `git diff --check` passed.
- Gracefully restarted the macOS app with session recovery intact. Confirmed Add link and Add file in the live Node inspector and left that panel visible for manual testing. No packages or remote deployments were produced.
