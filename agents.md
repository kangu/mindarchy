## Run after each task
- Persist inside docs/progress folder the Implementation Summary dumped in the terminal for the current task. Between the Initial Prompt and the Implementation Summary, also include the Plan that was followed to perform the implementation, then the proposed Next Steps.
  The filename should be the normalized iso datetime timestamp plus the extracted relevant title for the feature/task that was worked on.
- MacOS rebuild
  - After each completed task where the application needs to be restarted, close any running instances and re-run the latest build.

## Keyboard shortcuts
- Whenever a keyboard shortcut is added, changed, or removed, update the Help → Keyboard Shortcuts window in the same change. Keep its key combination, action description, and context accurate for the implemented behavior. The native macOS shortcut table currently lives in `src/macwindow.mm` (`OMMHelpTarget`). Update relevant shortcut documentation and verification alongside it.
