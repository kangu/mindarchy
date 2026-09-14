# Smooth automatic layout reflow

## Initial Prompt
Animate node and connection position changes smoothly when adding children or siblings in automatic layout.

## Plan
Trace existing interpolation and editing lifecycle. Keep creation animations active while editing, share animated geometry with the editor and connections, and retain reflow on title commit. Verify canvas and native UI behavior.

## Next Steps
Reopen the rebuilt macOS app for manual testing. Automatic restart remains unavailable because the computer-use connector rejects the current org.mindarchy.app identity. Remote platforms were not deployed.

## Implementation Summary
Removed the edit-start cancellation that snapped the entire graph to its target layout immediately after node creation. The editor now follows interpolated node positions while retaining live draft dimensions. Automatic commits allow reflow to finish without snapping the viewport; manual commits retain their existing anchor behavior. Existing connection rendering uses the same interpolated rectangles. Added regression coverage for creation, intermediate motion and commit during reflow, and updated inline editor expectations.

macOS build succeeded. Canvas and UI targeted checks passed. Seven of eight full CTest suites passed; the macOS document-close test failed once with native UI-server warnings and then passed on isolated rerun. git diff --check passed. No installer was built.
