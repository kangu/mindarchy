# Marquee fill 90% transparent

## Initial Prompt
Keep the selection rectangle outline; make its interior 90% transparent.

## Plan
Enable scene-graph blending so vertex alpha is honored, and set the marquee fill to 10% opacity.

## Next Steps
None.

## Implementation Summary
The marquee outline is unchanged. Its fill is 10% opaque (90% transparent) on GPU and software paths. Rebuilt and restarted macOS. Full suite skipped.
