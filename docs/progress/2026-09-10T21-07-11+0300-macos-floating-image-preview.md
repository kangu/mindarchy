# Initial Prompt
Use a lightweight macOS image preview, like the supplied reference, instead of a full window with buttons.

# Plan
Make the macOS preview a frameless floating tool panel, size and position it around the image, retain keyboard dismissal, and verify its appearance and canvas return.

# Next Steps
Select an image and press Space to try the updated macOS panel.

# Implementation Summary
The macOS image preview now has a rounded translucent dark frame, no title bar or window buttons, proportional sizing bounded to the display, and positioning over the document. Space/Escape dismiss it; clicking back on the canvas dismisses it too. Other platforms retain their existing preview window. Updated README.

Validation: Cocoa tests passed for floating/frameless flags, repeated Space and Escape toggling, canvas-click dismissal, preserved viewport/document revision, and image drop/resize/preview. Inspected the captured panel image. Rebuilt and restarted macOS with document recovery. No installer or other-platform deployment.
