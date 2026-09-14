# Initial Prompt
After saving, fade out Edited and smoothly move the document name down into its centered position in a synchronized animation.

# Plan
Use one animated progress value for label opacity and title position, maintain stable header geometry, and verify the save transition and existing folder hover interaction.

# Next Steps
Reopen the rebuilt app to load the update. Automated restart remains unavailable due to the computer-use connector's cached old bundle identity.

# Implementation Summary
Added a shared 240ms ease-in/out transition. Edited remains present until its opacity reaches zero, while the name moves down to the toolbar center using the same progress value. The title region retains its height to prevent layout snapping. Returning to an edited state reverses the transition, and folder hover positioning follows the animated name.
macOS build and documentHeaderTracksSaveAndEditing UI test passed. The test checks intermediate fade/movement synchronization, final centering, and folder hover/menu behavior. git diff --check passed.
