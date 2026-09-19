# Floating Rate Display alongside the taskbar Rate Strip

Win Mon offers an optional Floating Rate Display alongside its embedded Rate Strip. It is a draggable, always-on-top, taskbar-free desktop surface because the taskbar strip is intentionally unavailable when the primary taskbar cannot host it; its persisted absolute position returns to the primary work area's upper-right corner only when no usable portion remains on any current display.

The display uses Direct2D/DirectWrite software rendering into a per-pixel-alpha layered window. A separate mouse-transparent shadow window lets shadow clicks reach other processes without making the body noninteractive. It shares Network to Monitor and Rate Font with the Rate Strip, while fitting its own font sizes and offering the same Operator Menu on right-click. Show Floating Display and desktop position persist under ADR-0007.

Rate Samples remain once per second. A separate approximately 30 FPS timer animates activity and hover transitions, stopping when idle with no transition or when hidden, and respecting Windows' animation preference. Reusable text layouts, sample geometry, shadow pixels and presentation buffers keep animation frames from rebuilding unchanged resources; combined preview pixels are generated only when requested.
