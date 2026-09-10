# Node images — proposed design

Status: approved and implemented, with macOS and Omarchy verification.

## Request
Accept an image dragged onto a node, render it left of the node content, and resize it proportionally by dragging its edges. Inspect MindNode Classic and reproduce useful interactions.

## Reference observations
Inspected a separate MindNode Classic document through computer use:
- Pasting a PNG file onto the selected node attached it without replacing its title.
- Clicking the image selected it separately and displayed handles at the horizontal edge midpoints.
- Dragging an edge preserved aspect ratio, scaled around the image center, and resized the containing node.
- Its inspector offered image placement controls; leading placement rendered the image next to the text.
- Image context menu offered Quick Look, Edit Image, Cut, Copy, Paste, and Delete Image.
- Delete Image kept the title and shrank the node. Undo restored the image and its dimensions.
- Direct external drag/drop and replacement behavior have not yet been verified in the reference app. These are not represented as observed findings; actual file drop delivery is verified in Mindarchy on Cocoa and Wayland.

## Proposed behavior
- One image per node, including task and date nodes. It appears left of the existing node content with consistent padding and gap, vertically centered.
- Dragging a supported local image over a node highlights that exact target. Dropping attaches the image; dropping onto an existing image replaces it as one undoable action.
- Empty-canvas drops do not silently attach to the selected node. Unsupported or invalid input leaves the document unchanged and gives a clear message.
- Click an image to show selection handles. Drag any edge or corner to scale proportionally, with a live preview. Distinguish resize gestures from branch dragging. Escape cancels; release commits one undo step.
- Default displayed longest side: 120 logical pixels, bounded by source dimensions to avoid needless upscaling. User resize range: 24–1024 logical pixels on the longest side.
- Resizing updates node dimensions and connections; automatic layout reorganizes affected branches, while manual layout retains placement offsets. Text keeps its natural wrapping width rather than losing space to the image.
- Provide Preview, Replace Image, Copy Image, Reset Image Size, and Remove Image actions. Removal preserves text, task state, resources, and children.
- Preview is an application-owned image window so behavior can be shared across platforms. Native external editing, camera import, scanning, cropping, and multiple image positions are outside this first implementation.

## Storage and integration
Recommend embedded image data over external paths or sidecar files: it keeps a single portable .omm document, at the cost of larger files.
- Store normalized PNG/JPEG bytes as base64 plus display dimensions in an optional image field. .omm remains regular JSON.
- Preserve transparency and orientation. Decode using QImageReader with byte and decoded-pixel bounds; reject malformed or excessive images before committing.
- Keep decoded-image caching and image serialization in a dedicated image module. The node model owns immutable image content and its display size.
- Integrate image dimensions into shared node measurement, rendering, editing offsets, selection hit testing, and exports.
- Save/open, recovery, undo/redo, branch copy/paste, and PNG/Quick Look preview generation must retain the attachment.
- Existing documents without images continue to load unchanged. Document size limits must account for embedded images explicitly.

## Verification
Engine tests: import validation, aspect ratio bounds, measurement, persistence, recovery, undo/redo, and branch copy/paste.
Canvas/UI tests: drop targeting, replacement, proportional drag at different zoom levels, gesture cancellation, one-step undo, and preservation of normal branch dragging in both layouts.
Export checks: attachment appears in the PNG export and saved preview.
Build and manually inspect the macOS app using a disposable document; gracefully restart the current build. No installer or remote deployment is included.

## Approved compression addition
Imports retain at most 2048 pixels on the longest side. PNG preserves transparency and economical line art; JPEG quality 88 is used for opaque images when at least 20% smaller. Originals on disk are untouched. Compression tests measured 13,525,485 bytes to 1,360,845 bytes on a 3000×1500 opaque fixture.
