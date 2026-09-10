## Initial Prompt
Implement image drag/drop onto nodes, left-side rendering and proportional edge resizing, using useful MindNode Classic interactions. Test on macOS and Omarchy and compress large images for mind-map use.

## Plan
1. Add and exercise failing persistence/drop/preview tests.
2. Implement a shared embedded-image codec, compression and engine operations.
3. Integrate canvas image rendering, resize previews, drop targeting, task/calendar/text geometry, context actions and preview windows.
4. Verify save/reopen, recovery, branch clipboard, undo and cross-platform previews.
5. Build/test Cocoa and Wayland, update runnable builds and document results.

## Next Steps
No required implementation work remains. On Omarchy, start a new app window to load the updated binary; existing windows were preserved to protect unsaved work. No installer was generated and no commit was made.

## Implementation Summary
- Added src/nodeimage.h with immutable shared pixels/encoded data, bounded image reading, base64 JSON persistence, and compression. Imports cap the longest side at 2048 pixels. Transparent images remain PNG; opaque images use JPEG quality 88 when at least 20% smaller. Original files are never changed.
- Added one image per node, left-side content layout, engine attach/replace/resize/reset/remove/copy operations and per-document image budgets. Save/open, recovery, copied branches and PNG/Quick Look previews retain images.
- Added src/canvasimages.cpp for shared Qt file/pixel drops, highlighted targets, separate image selection and eight proportional resize handles. Resize previews remain outside the document until release and commit one undo step. Escape cancels. Center-anchored handles track pointer movement; regular branch dragging remains available.
- Added qml/NodeImageTools.qml for resize overlays, Preview/Replace/Copy/Reset/Remove actions and an application-owned preview window. Double-click previews; right-click opens actions.
- Kept task checkboxes, calendar controls, text editing and text wrapping correctly offset beside images. Verified detail text returns after overview zoom.
- Updated both shortcut dialogs for Escape during image resizing and documented the feature/compression limits in README.
- Watched persistence, drop acceptance and preview-image tests fail before implementing their missing behavior; subsequently all passed.
- macOS: all seven regression suites passed (69.66 seconds). Additional final Cocoa UI tests and focused canvas tests passed after final refinements. Computer use verified the image and context menu in the rebuilt application; the test document was closed and user documents restored through graceful Cmd+Q/relaunch.
- Omarchy 4.0.2, Qt 6.11.2: engine 57 passed; canvas 24 passed; UI 47 passed with two platform-specific skips; preview 5 passed. Subsequent focused tests cover the final handle-tracking and overview/task/calendar refinements. A real Wayland UI test verified local-file drop delivery, mouse-driven resize, screenshot capture and preview. The final app also passed a Wayland startup/screenshot/exit smoke test.
- Cross-platform check: each platform rendered a document saved on the other platform with its embedded image intact.
- Compression fixture: a 3000x1500 opaque PNG decreased from 13,525,485 bytes to 1,360,845 bytes (about 90%), stored at 2048x1024. Alpha preservation was separately tested.
- Updated Omarchy's standard build/mindarchy atomically, preserving existing processes. Test evidence is under artifacts/node-images/macos and artifacts/node-images/omarchy.
