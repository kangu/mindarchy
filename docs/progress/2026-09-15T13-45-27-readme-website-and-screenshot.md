# Initial Prompt

Mention www.mindarchy.xyz and include a prominent application screenshot near the top of the qt-prototype README.

# Plan

Inspect the site and available product images, choose a readable app capture, add the website link and image, and verify the Markdown and local asset.

# Next Steps

The exact live-site screenshot can replace this capture if the user supplies its page. Configure the www hostname to resolve if it is intended as a public entry point.

# Implementation Summary

Added the website link and a prominent, linked Forest-theme app screenshot near the top of README.md. Preserved the Omarchy APP badge. The image is an existing local app capture of the website’s bookshop example, copied unchanged into assets/screenshots so GitHub rendering does not depend on the website image host. The www hostname did not resolve during verification; the displayed website address links to the reachable https://mindarchy.xyz. Visually inspected the screenshot, verified the local image reference and byte-for-byte copy, and ran git diff --check. No application rebuild is needed for this documentation-only change.
