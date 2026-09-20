# Collaboration Undo Checkpoint

Added an actor-owned compensation manager over the Automerge adapter. Scalar
changes and text insertions record semantic before/current values, undo only
when the current value still satisfies the operation precondition, and support
redo after a successful undo. A peer change returns `undo_conflict` and is not
overwritten. Unicode text positions use Automerge's codepoint indexing.

This is intentionally conservative until stable Automerge text element
identities are exposed through the selected binding. Text undo currently
requires the complete recorded text value to remain unchanged, so a peer edit
anywhere in that text produces a conflict rather than risking deletion of the
wrong characters. It is not yet wired into the Qt engine or persisted local
store.
