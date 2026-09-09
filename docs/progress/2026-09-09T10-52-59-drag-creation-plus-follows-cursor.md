# Initial Prompt
Show the + symbol underneath the cursor while dragging a new child connection.

# Plan
Update shared handle geometry during an active drag, rebuild, and verify canvas interactions and native rendering.

# Proposed Next Steps
Try the updated development app.

# Implementation Summary
The creation + handle now follows the drag endpoint, centered under the cursor at a constant 24-pixel size. Both software and native renderers use the shared handle geometry. macOS development build, canvas suite, and native preview rendering check passed; screenshot visually checked. No packages built.
