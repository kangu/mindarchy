# MindNode Classic evaluation and feature proposals

## Initial Prompt
Evaluate MindNode Classic and propose additional Mindarchy features.

## Plan
Inspect the installed Classic app through native accessibility menus, verify Classic-specific capabilities against official sources, and compare with Mindarchy's engine and UI. Propose additions without implementing them.

## Next Steps
Recommended first sequence: branch clipboard interchange, branch Focus mode, then full-map/selection export. Follow with tags and linked resources.

## Implementation Summary
Evaluation only; no application code changed. Inspected Node, View, File and Edit menus and the Tags sidebar. Restored sidebar visibility and closed menus. Native screenshots were unavailable, so this was interface/accessibility inspection, not a visual usability benchmark or end-to-end test of each command.

## Findings and proposals
| Priority | Addition | Proposed Mindarchy behavior | Relative effort |
|---|---|---|---|
| 1 | Branch copy/paste and duplicate | Copy selected subtrees between windows, preserving notes, task/date data, styles and internal relationships. Paste indented text/Markdown as children. Remap IDs; handle connections leaving the copied branch explicitly. | Medium |
| 2 | Focus mode | Spotlight a selected branch and its ancestors, dim unrelated content, show a breadcrumb, and exit back to the exact prior viewport. Keep focus transient, without editing the document or fold state. | Medium |
| 3 | Full-map and selection export | Choose viewport, entire map or selected subtree. Add PDF/SVG, transparent PNG and copy-as-image. Use OPML/Markdown for initial interchange; state which visual metadata cannot round-trip. | Medium–large |
| 4 | Visual tags and filtering | Colored labels such as Decision, Blocked and Question across unrelated branches. Click a tag to highlight matches, with optional task-state filters. | Medium |
| 5 | Node links and linked resources | Attach URLs and local files, show an explicit open action, and support relative paths. Later add links to other .omm files and specific node IDs with missing-target handling. | Medium |
| 6 | Images on nodes | Drag or paste an image, resize it and optionally caption it. Preserve single-file JSON using bounded encoded assets; deduplicate repeated images and enforce file/decoded-pixel limits. | Large |
| 7 | Balanced layout and branch commands | Left/right children around the root; move a branch between sides. Add insert-parent, sibling-before, indent/outdent, unfold-one-level, fold-completed and sort-by-task-state commands. | Mixed |
| 8 | Quick Entry | A compact outline capture window, initially in-app. Turn indentation into a new map or insert beneath the current node. Add platform-specific global activation later. | Medium, larger across all OSs |
| 9 | User themes and reusable branch templates | Save current styles as a named theme and any selected subtree as a template; preview and share via JSON. | Medium |
| 10 | Version history | Browse dated local snapshots and restore as a copy; distinguish historical versions from existing crash recovery. | Medium–large |

Effort labels are comparative estimates, not schedules. Focus navigation, breadcrumb behavior and branch import/export details above are proposed designs, not claims about Classic's exact implementation.

## Small additions worth grouping
Zoom to selection; copy selected nodes as transparent PNG; fold all completed tasks; find and replace with preview and one undo transaction; insert parent; sort siblings alphabetically or by task state. Mindarchy already has tasks/progress, notes, relationships, folding, automatic/manual layout, calendar nodes, templates, fuzzy search and themes, so those are not proposed as new capabilities.

## Evidence
The installed Classic menus exposed Focus, Add Link/Open Linked File, Balance Children, Move Children Left/Right, sorting, unfold one level, fold completed tasks, copy selected nodes, copy-as-image with transparent background, paste-and-preserve-style, find-and-replace, PDF/FreeMind/OPML/Markdown/Text/Tasks export and native version browsing. Tags sidebar exposes document tag management. Quick Entry, images, stickers and personal themes are also documented in the Classic App Store listing.

Sources:
- https://apps.apple.com/us/app/mindnode-mind-map-outline/id1218718027?mt=8
- https://www.mindnode.com/classic

Classic is distinct from the current MindNode app: the official comparison lists live collaboration and conflict-free sync under the newer app. Those should not be presented as Classic features. For Mindarchy, collaboration/cloud sync and large sticker libraries are lower priority than navigation and portable document workflows.
