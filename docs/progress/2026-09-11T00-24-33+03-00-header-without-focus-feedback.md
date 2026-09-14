# Initial Prompt
Header buttons should never retain a focus outline or show a focus-triggered tooltip after a dialog closes.

# Plan
Separate focus feedback from button state, disable it for header buttons, keep hover and checked feedback, and verify in the isolated side-task build.

# Next Steps
The main application's next build will include the change. The running app was not restarted to avoid disrupting the main conversation.

# Implementation Summary
Toolbar buttons now use NoFocus and opt out of focus outlines and focus-triggered tooltips, including the zoom and Omarchy menu overrides. Header search-close and Windows caption buttons also avoid focus feedback. Existing hover tooltips and checked-state styling remain. Inspector IconButtons retain their existing keyboard focus behavior.
Built the macOS app in build-side-manual-placement. The new regression test deliberately restores focus to Open, Save, Zoom, Search and Templates and checks zero outline and no tooltip without hover. Active toggle styling, zoom popup actions and search shortcuts remain verified. All 5 UI test results passed; git diff --check passed. No actual document dialog was opened against user data.
