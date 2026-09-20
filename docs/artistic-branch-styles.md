# Artistic branch styles

Choose **Inspector → Map → Connections** to select a style. The choice applies to the whole map and is saved in `.omm` documents, recovery snapshots and undo history.

| Style | Treatment |
| --- | --- |
| Rounded | Existing smooth, constant-width connections. |
| Angular | Existing right-angle connections. |
| Botanical graphite | Tapered stems, graphite outlines and fine longitudinal grain. |
| Living oak | Warm layered wood, bark marks and small green buds. |
| Sumi branch | Dark brush-like silhouettes with fine broken-looking ink fibers. |
| Silver birch | Ivory stems, subtle highlights and contrasting bark marks. |
| Elven filigree | Very fine sage/silver branches, outlined leaves and curling tendrils. |

Artistic styles are procedural vector interpretations of the design samples, not embedded image textures. They follow horizontal, vertical, compact and manually positioned branches. Node positions, text, shapes, task controls and cross-links retain their existing behavior. Node underlines retain the theme’s normal line treatment.

Each artistic style automatically uses its light or dark palette according to the actual map canvas color. For example, compare Sage with Graphite. This is independent of the application shell or operating system appearance. Existing branch tint and width contribute to the result; width zero still hides the connection. Artistic strokes are continuous organic shapes; node-level dash/dot settings continue to apply to legacy connections and node underlines rather than cutting up the artistic silhouette.

Details are seeded by node ID so dragging, animation and repainting do not randomize the texture. Fine grain and ornaments are omitted below 48% map zoom, preserving the main silhouette. Very short connectors omit ornaments to protect legibility. Software canvas, GPU canvas and document previews use the same vector geometry; PNG export captures the active canvas.

The serialized `branchStyle` values are the names in the table. Earlier Mindarchy versions that recognize only Rounded/Angular cannot open documents containing these new values; use Rounded or Angular before saving a copy for an older version.

## Examples

Open the maps in [examples/branch-styles](../examples/branch-styles) to compare each style against Sage (light) and Graphite (dark). The Elven filigree pair is a useful starting point for the thinnest treatment.
