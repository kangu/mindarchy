## Run after each task
- Persist inside docs/progress folder the Implementation Summary dumped in the terminal for the current task. Between the Initial Prompt and the Implementation Summary, also include the Plan that was followed to perform the implementation, then the proposed Next Steps.
  The filename should be the normalized iso datetime timestamp plus the extracted relevant title for the feature/task that was worked on.
- MacOS rebuild
  - After each completed task where the application needs to be restarted, close any running instances and re-run the latest build.

## Keyboard shortcuts
- Whenever a keyboard shortcut is added, changed, or removed, update the Help → Keyboard Shortcuts window in the same change. Keep its key combination, action description, and context accurate for the implemented behavior. The native macOS shortcut table currently lives in `src/macwindow.mm` (`OMMHelpTarget`). The Windows table lives in `qml/KeyboardShortcuts.qml`. Update relevant shortcut documentation and verification alongside it.

## Development build and test policy
- Use `python3 scripts/dev.py build` for the app and `python3 scripts/dev.py check` for normal code changes. Reuse the existing build directory; do not rebuild with qmake in per-suite directories.
- Do not run the full UI suite or native desktop tests after every change. Run targeted offscreen UI cases only when the changed interaction warrants them; use `dev.py ui` for broad QML changes.
- Reserve `dev.py full` for release validation or explicit requests to diagnose native window behavior. `dev.py full` runs fast+offscreen suites only (no windows); `dev.py native` runs the window-management suites and is opt-in for developers / mandatory before release packaging (release scripts run both legs). Do not launch visible test replays unless requested.
- Build/script/documentation-only changes do not require restarting the user's app. For application changes, restart only once after relevant checks pass, preserving unsaved sessions.
- Do not run installer packaging, deployment, signing or notarization for ordinary development builds. Release packaging retains full test coverage by default.
