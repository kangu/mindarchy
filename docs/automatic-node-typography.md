# Automatic node typography

Node size follows tree depth: 20 px at the root, then 18, 17, 16, 15 and a 14 px minimum. Zoom scales these logical sizes as usual. Reparenting recalculates the entire affected branch, including folded descendants.

The node inspector reports the automatic size. Font family, bold, italic, underline, strikethrough, text color and alignment remain customizable. Manual font-size changes are no longer offered. Existing and pasted rich text uses the automatic size while preserving its other formatting; opening a file does not rewrite its source text.

Measurement, editing, canvas labels, previews and exports share the size policy. Calendar nodes use the same depth scale for their content and day hit targets. No document schema change is required.
