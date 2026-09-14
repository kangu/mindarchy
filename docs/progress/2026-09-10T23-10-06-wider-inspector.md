# Initial Prompt
Make the inspector slightly wider so the full Themes label is readable.

# Plan
Increase the inspector width, rebuild macOS and inspect its tab labels in a rendered UI screenshot.

# Next Steps
Quit and reopen the running application to use the rebuilt UI. Automatic restart remains blocked by the computer-use connector caching the old bundle identity.

# Implementation Summary
Increased inspector preferred width from 274 to 320 logical pixels. The macOS build and targeted inspector/calendar UI test passed. Visually confirmed that Map, Node and Themes fit without truncation in /tmp/mindarchy-wider-inspector/date-inspector.png. git diff --check passed.
