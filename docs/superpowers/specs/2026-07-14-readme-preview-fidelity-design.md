# README Preview Fidelity Correction

## Goal

Replace the first README showcase previews with documentation assets that
accurately mirror the current firmware and dashboard layouts.

## OLED previews

The existing SVG gallery is an approximation and must be replaced.

Create a documentation-only renderer that:

- Creates an actual 64×48 monochrome framebuffer for each static preview.
- Uses the same current layout coordinates, labels, menu order, and interaction
  states as `firmware/src/main.cpp`.
- Reuses the firmware XBM icon arrays and U8g2-compatible visual treatment.
- Scales output with nearest-neighbor pixels only after rendering the real
  64×48 bitmap.

Preview states:

1. Menu
2. Measure
3. Time main
4. Time detail
5. Weather main
6. Weather detail
7. Room main
8. Room detail
9. Timer
10. Timer complete alert

Generated outputs replace `docs/pics/readme/oled-ui-gallery.svg` with a PNG
gallery. The source renderer belongs under `docs/pics/readme/source/`.

## Dashboard preview

Replace the generic chart region in `dashboard-preview.svg` with a faithful
current ECharts-style trend panel:

- Time labels on x-axis
- Temperature scale on left
- Humidity and light scales on right
- Current indigo / sky / amber series and legend
- Grid, card, and axis styling consistent with `TrendChart.tsx`

The preview remains explicitly a documentation interface preview because the
local environment has no populated Supabase data.

## Validation

- Compare every OLED preview layout with current `main.cpp` functions.
- Verify generated bitmap dimensions are 64×48 before scaling.
- Verify nearest-neighbor scaling produces crisp OLED pixels.
- Confirm README points to the new PNG OLED gallery and corrected dashboard SVG.
- Remove the obsolete OLED SVG gallery from the repository.
