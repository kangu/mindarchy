# Mindarchy — Refined theme collection

Goal: add at least five polished themes, select the strongest new-document default, and improve completed tasks. The scope also includes theme-controlled checkbox and progress-ring appearance.

## How the collection was assembled

Three useful sources of inspiration:

1. **Product finishes:** look at the relationship between a neutral surface and a restrained accent, rather than sampling a screenshot literally. Apple's [MacBook Air finishes](https://www.apple.com/newsroom/2025/03/apple-introduces-the-new-macbook-air-with-the-m4-chip-and-a-sky-blue-color/) informed Sky and Starlight.
2. **Editorial and natural palettes:** paper/ink, sage/stone, rose/plum, and charcoal/silver give each palette a coherent temperature. These are original combinations rather than imported theme files.
3. **Content-first interface design:** Apple's [iMac design](https://images.apple.com/uk/newsroom/2021/04/imac-features-all-new-design-in-vibrant-colors-m1-chip-and-45k-retina-display/) uses softer front-facing colors to keep attention on content. The same restraint here means pale topic fills, clear ink, and six distinct branch colors.

Validate each direction on the same document, then on real controls. Swatches alone hide weak text contrast, overly vivid branches, and unclear task status. The shared examples include short and long labels, three hierarchy levels, unchecked/completed tasks, and partial/completed parent progress.

## Collection

| Theme | Canvas | Ink | Character | Completion accent on the canvas |
| --- | --- | --- | --- | --- |
| **Porcelain — default** | `#F7F8FA` | `#273444` | Cool white, slate anchor, quiet jewel accents | Emerald `#147D64` |
| Sky | `#F2F7FB` | `#263C50` | Air blue, pale blue center, deep navy text | Blue `#2876A2` |
| Starlight | `#FAF7F1` | `#403A32` | Warm ivory, champagne center, bronze accents | Bronze `#8B692E` |
| Sage | `#F4F7F3` | `#2C3D34` | Mineral green, forest center, botanical ink | Green `#39754F` |
| Blush | `#FCF6F5` | `#46343F` | Rose porcelain, plum ink, restrained pink | Rose `#A34F73` |
| Graphite | `#20242B` | `#E6E9EF` | Charcoal, soft silver center, desaturated accents | Silver blue `#A7C7EC` |

Porcelain wins as the default because it works as a neutral working surface while providing a strong central anchor. Sky and Blush are more visibly tinted; Starlight is warmer; Sage has more botanical character. Graphite is the dark counterpart. This is an editorial choice, not an objective ranking.

All existing themes remain available. Newly created documents use Omarchy on detected Omarchy systems and Porcelain elsewhere. A document's saved theme and authored font formatting are preserved.

## Typography

On Omarchy systems, Omarchy leads the picker and Porcelain comes second. Elsewhere their order is reversed. Its dark canvas and terminal accents draw from [Omarchy’s Tokyo Night palette](https://github.com/basecamp/omarchy/blob/master/themes/tokyo-night/colors.toml), with squarer node corners and green task indicators. This saved document palette complements the live system-themed Omarchy shell.

Every theme has its own bundled, open-source base family. The collection deliberately mixes geometric and humanist sans faces, editorial serifs, and monospace styles. Fonts are embedded in the application, so these choices do not depend on fonts installed on the user's computer.

| Theme | Base font |
| --- | --- |
| Porcelain | Inter |
| Omarchy | JetBrains Mono |
| Sky | Manrope |
| Starlight | Lora |
| Sage | Source Sans 3 |
| Blush | DM Sans |
| Graphite | IBM Plex Sans |
| Beach Day | Nunito Sans |
| Holographic | Space Grotesk |
| Retro | Archivo |
| Arcade | Space Mono |
| Lab | IBM Plex Mono |
| Canopy | Literata |
| Atlas | Public Sans |
| Studio | Source Serif 4 |
| Nocturne | Plus Jakarta Sans |
| Paper | Newsreader |
| Forest | Lato |
| Midnight | Outfit |

The 15px base remains readable and compact. The example maps use authored 22px semibold central topics, 16px medium branch headings, and 15px regular details. Explicit per-node families, sizes and weights remain authoritative. Theme changes remeasure node geometry and update canvas rendering, editing, calendar text and exports together. Ordinary editing or bold formatting retains theme inheritance.

Original font files and their SIL Open Font Licenses live in `assets/fonts`. The manifest records the pinned [Google Fonts source](https://github.com/google/fonts) revision and file hashes. No Apple font files are distributed. Apple's [typography guidance](https://developer.apple.com/design/human-interface-guidelines/typography) informs legibility and hierarchy.

## Task and progress design

`TaskAppearance` defines completion color, completed-frame opacity, tick weight, corner radius, track opacity and progress stroke width. Theme recipes select those values; the resolved fill chooses a contrasting light/dark variant of the theme's completion accent. Custom node fill overrides are included in that contrast decision.

Completed leaf tasks retain an empty, faded frame with a larger rising tick. Partial parent progress uses a muted track and rounded-ended arc. A fully completed parent adds a small check inside its complete ring. Geometry and colors are shared between the canvas, software rendering and image exports. Theme cards use the same theme metadata for their task examples.

## Validation targets

- Text contrast at least 4.5:1 against every default node surface, across all six branches and four depths.
- Branch and completion accents at least 3:1 on their relevant default backgrounds.
- Filled calendar days choose black or white with the greater contrast.
- Theme switching, undo, persistence, previews, task interaction and editor font parity remain covered by tests.
- Inspect all six sample exports, plus actual application screenshots.

Scope: shared desktop code; the current run is built and tested on macOS. Omarchy and Windows receive it in their next builds. Omarchy's system-controlled application shell remains independent from canvas themes.
