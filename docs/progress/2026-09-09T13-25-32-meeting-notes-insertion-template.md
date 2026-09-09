# Initial Prompt
Move Meeting Notes from the node inspector conversion control into the node template insertion picker, and remove the old Meeting Notes node template controls.

# Plan
Register Meeting Notes in the template catalog. Replace in-place conversion with atomic child-branch insertion. Hide weekly calendar settings for this template. Remove the inspector conversion button while retaining metadata editing for existing meeting branches. Verify insertion, persistence, undo, prompts, and both template UI flows.

# Proposed Next Steps
Restart the rebuilt macOS app and choose Add node template → Meeting Notes.

# Implementation Summary
Meeting Notes now inserts a new child beneath the selected node, with Agenda, Notes, Decisions, and Actions sections and one empty entry each. The first Notes entry opens with editing focus. Actions retain task behavior and section-specific prompts remain available. Insertion validates capacity and depth, preserves existing children and the selected parent's properties, and is one undoable operation. Multiple meeting templates can be inserted under the same parent. Removed the old applyMeetingTemplate conversion API and inspector button. The inspector shows meeting date/time/attendees only for meeting nodes, including existing saved documents. The picker hides all weekly calendar controls for Meeting Notes.

Rebuilt macOS. Focused engine tests passed for meeting insertion, existing-child preservation, save/reopen, undo/redo, repeat insertion, and the weekly template. Both template UI flows passed offscreen and on native macOS, including automatic editing focus, metadata editing, and absence of the old conversion control. git diff --check passed.
