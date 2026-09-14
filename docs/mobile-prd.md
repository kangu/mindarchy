# Mindarchy Mobile — Minimal PRD

Status: proposal for future development; no implementation scheduled.
Date: 2026-09-11

## Goal
Make mind maps useful on a small touch screen through comfortable branch navigation, rapid capture and task completion. Preserve access to the full structure without requiring desktop-style canvas manipulation for everyday work.

## Experience principle
“One branch in your hand, the whole map one gesture away.” Branch view is the primary working surface; map view provides orientation and broader structural context.

## First-release scope
- **Branch navigation:** show the current node as a heading, its children as large touch-friendly rows, and a compact ancestor breadcrumb. Tap a child to enter its branch; use Back or a back gesture to return. Restore the last focused branch when reopening a map.
- **Map overview:** a persistent Map action opens the complete canvas with pinch and pan. Selecting a node returns to its branch view. Keep selection and navigation context consistent between views.
- **Rapid capture:** a bottom input adds children to the current branch. Return adds the thought and clears the input for another. Support system dictation and keep the active input visible above the keyboard.
- **Tasks:** complete tasks directly in branch view. Show aggregate completion on parent progress rings using the existing task rules and active theme.
- **Search:** find nodes across the current map and navigate directly to a selected result, with matching text highlighted and ancestors visible for context.
- **Sibling reordering:** long-press to lift a row; dragging opens an insertion gap. Dropping changes sibling order and provides Undo. Provide an accessible alternative to dragging.
- **Document safety:** preserve the existing hierarchy, rich text, tasks, images and links when opening and saving supported maps. Recover unsaved edits after interruption. Unsupported editing features must not silently discard content.

## Haptic and motion language
| Event | Feedback |
| --- | --- |
| Start dragging | Light pulse; row lifts slightly with a soft shadow |
| Change insertion target | Subtle tick when the destination changes; visible insertion gap |
| Successful drop | Gentle settling pulse and short settling animation |
| Complete a task | Crisp pulse synchronised with the check animation |
| Complete a task branch | Slightly fuller confirmation as the progress ring closes |
| Invalid drop | Restrained warning pulse and visible explanation |

Do not vibrate during ordinary scrolling or navigation. Coalesce rapid target changes to avoid excessive feedback. Provide visual equivalents for every cue, an option to disable haptics, reduced-motion support and screen-reader labels/actions. Devices without haptic support retain complete functionality.

## Follow-up capabilities
- Long-press + menu for Task, Image, Link and Template; offer a discoverable tap-based alternative.
- Inbox for ideas that do not yet have a destination.
- Reparenting by holding a dragged node over a destination; Move to… sheet for distant moves.
- Weekly task view with day-to-day navigation and parent progress.
- Uncluttered image previews; notes and resources in bottom sheets that preserve map context.

## Acceptance criteria
1. A user can open a map, navigate into a branch, add several thoughts, complete a task and reorder siblings without using the full canvas.
2. Switching between branch and map views preserves the selected node and provides clear orientation.
3. Every structural or task change supports Undo; interrupted editing recovers without losing confirmed input.
4. Drag targets and task states remain understandable with haptics disabled and reduced motion enabled.
5. Primary controls have at least 44pt targets on iOS or 48dp on Android, support text scaling and remain usable with the keyboard open.
6. Round-trip document tests preserve content and relationships, including features mobile cannot yet edit.
7. Usability sessions validate one-handed capture, branch navigation and reordering on small phones; refine motion and haptic intensity on physical devices.

## Delivery plan
1. Prototype branch navigation, capture and map-to-branch transitions; test with representative maps and users.
2. Confirm mobile platform and rendering architecture, document compatibility, storage/recovery and haptic APIs.
3. Build the first-release scope with accessible interactions and Undo.
4. Validate on physical devices, including keyboard, interruption, large-map and accessibility scenarios; prioritise follow-ups from findings.

## Decisions before implementation
Choose initial platform(s), native versus shared UI approach, file access and optional sync strategy, and practical large-map performance targets. Cross-device sync and full desktop feature parity are outside the initial scope until explicitly planned.
