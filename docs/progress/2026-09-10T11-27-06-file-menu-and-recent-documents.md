# Initial Prompt
Provide a File menu with standard New / Open / Save functionality and a list of recent files for quick reopening.

# Plan
Inspect existing menu and session behavior; add independent shared recent history; wire native macOS and shared desktop menus; verify persistence, safe opening, and menu integration; rebuild and run updated desktop apps.

# Next Steps
Use File → Open Recent for quick reopening. Validate the shared menu on Windows during its next platform test session.

# Implementation Summary
Added native macOS File → New, Open, Open Recent, Save, and Close Window; extended the shared Windows/Omarchy File menu with Open Recent. History persists the latest 15 successful opens/saves in a separate locked system settings file, refreshes across windows, disables missing files, and supports Clear Menu. Opening from the dialog or recent list validates the document and requests a new window, preserving current unsaved work. Added history, QML menu, and native Cocoa menu regression coverage; documented behavior in README.

Validation: macOS build succeeded; engine, UI, and native macwindow suites passed (the initial engine expectation was corrected to account for canonical macOS temporary paths). Reopened the macOS app gracefully and inspected its native File menu and restored recent document. Omarchy build succeeded; targeted recent/shared-menu tests passed offscreen. Updated Omarchy app launched successfully under Wayland, mapped on workspace 2 with a clean runtime log. Windows uses the shared menu implementation but was not run in this task. No installers built.
