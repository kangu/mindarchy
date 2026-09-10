# Type to replace selected node title

## Initial Prompt

When a selected node says "hello", typing "yey" should immediately enter editing and replace the title with "yey".

## Plan

Route printable canvas input into the existing inline editor, preserve the first character and normal commit/undo behavior, resolve single-character shortcut conflicts, and verify on macOS.

## Next Steps

Manual testing in the restarted macOS app. Date nodes retain their calendar editor; Space remains the pan gesture. Input method composition and hardware keyboard layouts were not separately tested.

## Implementation Summary

- Typing with one text/task node selected starts the normal inline editor, replaces its title with the first character, and leaves the cursor at the end for subsequent typing. HTML-sensitive characters are escaped. Existing title baseline and single-transaction document undo are preserved.
- Command/control combinations and input inside other fields retain their existing behavior. No selection or multiple selections do not start replacement editing. Space-drag remains panning.
- Moved Fold and Task shortcuts from F/T to Alt+F/Alt+T so those letters can begin titles. Bare +, −, and 0 zoom commands now apply only with no node selected. Updated macOS and Windows Help tables and README.
- Added UI coverage for hello→yey, undo restoring hello, F/T/numeric/punctuation titles, literal markup, Alt+T, no-selection typing, and Space. Updated existing folding tests to the new shortcut.
- Rebuilt macOS. Engine, canvas, UI, window placement and preview suites passed in the full run; a transient native Help test failure passed on direct rerun and CTest rerun (2.20 seconds). The real Cocoa typing integration passed after retrying an intermittent test-window focus failure. Test initialization now explicitly activates Cocoa windows. No diagnostic instrumentation retained.
- `git diff --check` passed. Gracefully restarted the latest macOS app with the existing document restored. No installer or remote deployment was produced.
