# Initial Prompt
Make the Omarchy image preview borderless and clarify whether macOS can use the system Open in Preview toolbar.

# Plan
Use a decoration-free Linux image overlay, preserve Mac behavior, verify Wayland preview interaction, and update the remote binary.

# Next Steps
Reopen an Omarchy window to load the updated preview. Native macOS Quick Look remains a possible separate integration; the existing Mac panel is custom.

# Implementation Summary
Omarchy now uses a modal in-app Popup.Item with a transparent empty background and zero padding, so there is no compositor window border, title bar, or preview buttons. It fits the image and supports Space, Escape, and outside-click dismissal. Updated README and platform-specific preview tests. A separate Wayland grabbing popup was tried but replaced with the reliable in-app overlay after testing exposed its real-input serial requirement.

Real Wayland Space/Escape and image drop/resize/preview tests passed. Atomically installed the updated Omarchy binary without interrupting existing windows. No installer created.
