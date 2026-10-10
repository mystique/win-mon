# Floating LED display implementation

Completed 2026-10-11 in dependency order: 01 → 02 → 03 → 04.

- Symmetric two-ended semicircles; original ring outer diameter 40.36 DIP, chart width 64.58 DIP (exact 5:8), aligned independent chart clips, faint grids, gradient fills and 32 real samples.
- Embedded pinned Punctum glyph subset, nine fixed 35-lamp cells, right-aligned 999.9 capacity and saturation, with Rate Strip font configuration preserved and floating font APIs removed. Actual-size calibration uses round lamp diameter 72% of pitch to improve coverage at 96 DPI. Source attribution and full OFL are in the header and distributed `licenses/Punctum-OFL.txt`.
- Traveling waves retain visible high-activity waterlines. Water and reversible width/opacity share one native timer, with hidden/idle and disabled-animation handling.
- Show Floating Speed Text is connected through core/menu, live transaction and registry. Body, shadow, hit testing and buffers follow animated width. Failed reverse application resumes prior transition; work-area-adjusted positions notify persistence only after successful final presentation.

## Verification

CMake configured x64 and built Release. Release CTest: 8/8 passed. After final evidence-only test change, FloatingRateRendererTests was rebuilt and passed again.

Existing public renderer/window/menu/settings/font-transaction seams cover symmetric rings, zero/invalid input, separate clipped curves, 32-sample expiry, all glyphs and 35 lamp positions, blank leading cells, overflow saturation, unknown-input frame preservation, phase-only water changes, DPI frames and unchanged chart/ring pixels during width transitions. Real window tests cover shadow/no-activation, drag constraints, intermediate widths/reversal, rendering failure recovery, compact hit testing, work-area expansion and final position notification, hidden/reopened preference, disabled animations and repeated resource cleanup. Registry tests run with process-local isolated HKCU; temporary animation preference is restored by scope cleanup.

Two-axis code review found one rollback issue and one final-position persistence issue; both were fixed and reviewers confirmed closure. No remaining reported Standards or Spec findings.

## Evidence and limits

[evidence/](evidence/) contains actual-size 96/120/144/192 DPI offscreen PNGs on dark/light backgrounds, native screenshots at the available monitor's 144 DPI, and native collapse/expand/wave GIFs. Native screenshots include the current desktop through transparent margins; GIF canvases retain the expanded bounds only for presentation, while actual window widths vary.

Physical cross-monitor DPI migration and dragging during a real DPI switch were not exercised because only the available monitor was used. The remaining small-display readability judgement should be checked on the user's preferred monitor; render geometry is DPI-scaled and tested offscreen. GitHub release publication and restart of the user's running instance were not performed; CMake copies OFL to the executable directory and the release workflow includes it in the zip.

Correction: restored pre-redesign ring diameter and stroke width, expanded charts at 5:8 and LED pitch by about 68%. Window height remains 48 DIP; visible body grows to approximately 217 DIP. Refreshed all visual evidence.

Decimal refinement: cropped the decimal to its middle three columns (21 lamps), preserving baseline and fixed slots. Numeric lamps now use brighter green/blue, with arrows, units and dark lamps unchanged. Removed two dot-pitch columns from text/window width; refreshed all evidence. Release x64 and all eight CTest targets passed.

Waterline polish: a narrow pale cyan highlight and two low-opacity wider strokes add restrained glow along the moving surface only. All strokes share the existing inner-circle clip, including the wave ends. Release build and all eight CTest targets passed; refreshed actual-size evidence.
