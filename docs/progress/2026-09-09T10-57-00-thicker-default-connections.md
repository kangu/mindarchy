# Initial Prompt
Make connection lines slightly thicker, matching node borders.

# Plan
Inspect shared appearance defaults, increase branch width, rebuild and run relevant tests.

# Proposed Next Steps
Check the updated connection appearance in the development app.

# Implementation Summary
Raised the shared default branch width from 1.5 to 2 pixels to match standard node borders. Applies to canvas rendering and PNG/Quick Look previews; explicit custom branch widths remain respected. Rebuilt macOS development app. Engine, canvas and preview suites passed; diff check passed. No installer packages built.
