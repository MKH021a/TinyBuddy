Round 6 - current renderer, ready for approval
No design-parameter changes vs round 5. Renderer fixes found while frame-stepping animation #80:
- safe area: tall eyes no longer leave the screen on a big leap (QA: edge pixels on A/B/C/F)
- starlight dither needs >= 2 rows (QA: 28 isolated pixels on F mid-blink)
- iris hidden only once the "^" arch has formed (no flashing solid white domes)
- a nearly closed eye is a solid thick line (no "outlined box" frames)
- straight closed-eye seam (no 1 px kink); effects clamped fully on screen
Inspected: this contact sheet, expressions_*.png, animation key poses + filmstrips for all six faces.
