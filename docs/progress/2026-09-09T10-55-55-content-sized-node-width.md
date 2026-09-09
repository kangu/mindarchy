# Initial Prompt
Remove the minimum node width so nodes fit their content and padding.

# Plan
Locate the measurement floor, align rendering and editing widths, rebuild and verify short and empty nodes.

# Proposed Next Steps
Use the rebuilt development app to check content sizing in existing documents.

# Implementation Summary
Removed the automatic 72-pixel text-width floor. Nodes now use rounded-up measured content width plus existing padding and task indicator space; explicit fixed widths and long-text wrapping remain supported. Canvas labels, inline editor and PNG preview now accept narrow text areas. Rebuilt macOS development app; engine, canvas, UI and preview suites all passed. Visually verified New idea, Small, Da, I and empty nodes using the native Metal app screenshot. No installer packages built.
