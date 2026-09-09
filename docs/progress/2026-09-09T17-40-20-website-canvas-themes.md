# Website canvas themes

## Initial Prompt
Add themes matching the website at http://127.0.0.1:6027/, including at least one that looks like its examples. Test on macOS only.

## Plan
Inspect the running site's appearance demonstration and CSS tokens. Add Paper, Forest and Midnight to the existing theme catalog with matching rounded outlines and branch-colored text. Verify readability, persistence, gallery activation, PNG export and macOS rendering.

## Next Steps
Choose Paper, Forest or Midnight from Inspector → Themes. Existing explicit node style overrides remain in effect. No installer or Windows/Omarchy deployment was performed.

## Implementation Summary
- Added Paper (#fbfaf6), Forest (#203b32) and Midnight (#253443) canvas themes in src/theme.cpp.
- Paper uses the website's green/rust/blue accents (#3e6b50, #9c492d, #375d80). Dark variants use the site's corresponding pale accents.
- All depths use 11px rounded corners, 1.5px outlines/connections and branch-colored text on canvas-matching fills, following the website's appearance example.
- Existing gallery discovers the new presets and generates previews automatically. Beach Day remains the default.
- Added contrast and undo/redo/save/reopen coverage; extended gallery activation/export tests to all three new themes.
- macOS build succeeded. Engine and UI suites passed: 45.62 seconds total. Visually reviewed native macOS captures for all three themes under artifacts/website-themes.
