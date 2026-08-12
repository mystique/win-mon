# 02 — Embed a static Rate Strip

**What to build:** Extend the working tray process with a display-only Rate Strip embedded beside the notification area on the primary bottom Windows 11 taskbar. It initially shows two fixed lines: upload on top as `0.0K/s ↑` and download below as `0.0K/s ↓`. Launch, shell embed, and Exit form a complete demonstrable path.

**Blocked by:** 01 — Launch Win Mon from the Tray Icon.

**Status:** resolved

- [ ] Launch embeds the Rate Strip as a child of the primary Windows 11 taskbar, anchored beside the notification area.
- [ ] The implementation uses the Shell_TrayWnd child strategy required by ADR-0001, not a free-floating always-on-top overlay.
- [ ] The Rate Strip is shown only for the primary bottom taskbar and is not created on secondary taskbars.
- [ ] The top line displays `0.0K/s ↑`; the bottom line displays `0.0K/s ↓`.
- [ ] The Rate Strip uses a system UI font and a fixed width capable of displaying a wide value such as `999.9G/s` without clipping or jitter.
- [ ] Text is readable against the active Windows 11 taskbar; theme-aware coloring is used when practical.
- [ ] The Rate Strip is display-only: left-click, right-click, hover, and keyboard focus expose no product interaction, menu, or tooltip.
- [ ] Exit destroys the Rate Strip, removes the Tray Icon, and terminates the process.
- [ ] Win Mon does not shrink, resize, or permanently alter the task list or any unrelated taskbar child.
- [ ] No classic-taskbar surgery such as modifying MSTaskSwWClass is introduced.
- [ ] TrafficMonitor may be read to understand the taskbar technique, but all shipped implementation is Win Mon-owned and no TrafficMonitor source or project enters the build.
- [ ] Manual Windows 11 smoke verifies the Rate Strip locus, two-line layout, lack of interaction, stable width, and clean Exit.
