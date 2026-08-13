# 05 — Recover from Windows shell and display changes

**What to build:** Keep Win Mon usable when Explorer recreates the taskbar, the primary monitor or DPI changes, or the supported primary bottom taskbar is temporarily unavailable. Restore shell surfaces automatically while retaining the in-memory NIC Selection and ongoing sampling state.

**Blocked by:** 04 — Select one NIC from the Operator Menu.

**Status:** resolved

- [ ] Explorer/taskbar recreation restores exactly one Tray Icon and one supported Rate Strip without a user prompt.
- [ ] Shell Recovery preserves the current in-memory NIC Selection and resumes its displayed rates.
- [ ] If the primary taskbar is missing or not bottom-aligned, the Rate Strip is hidden while the Tray Icon, Operator Menu, and Rate Samples remain operational.
- [ ] When a supported primary bottom taskbar becomes available again, the Rate Strip is re-embedded automatically.
- [ ] A primary-monitor change re-finds the primary taskbar and moves/recreates the Rate Strip there only.
- [ ] DPI changes relayout the Rate Strip so both lines remain readable and unclipped at the new scale.
- [ ] A Rate Strip embed failure does not terminate Win Mon while the Tray Icon remains usable.
- [ ] Embed failure is retried when the shell stabilizes, without a tight retry loop or duplicate strip.
- [ ] Tray Icon recreation does not leave duplicates or stale callbacks.
- [ ] Rate Strip recovery never resizes or alters unrelated taskbar children.
- [ ] Exit remains clean after one or more shell recovery, DPI, primary-monitor, hide, or restore cycles.
- [ ] Manual Windows 11 smoke covers Explorer restart, supported taskbar return, DPI or primary-monitor change where available, selection retention, and final Exit.

## Implementation notes

- `TrayMessageWindow` uses a hidden top-level `WS_POPUP` message sink, not `HWND_MESSAGE`, because the registered `TaskbarCreated` notification is a system broadcast.
- The tray icon is deleted and re-added after shell recreation; the Rate Strip is re-embedded while `WinMonCore` selection and sampling state remain in memory.
- During each one-second Rate Sample, an existing Rate Strip re-reads the notification-area geometry and moves itself beside the current tray edge. It also re-samples taskbar theme color while hidden and re-renders when the color changes.
- The dynamic refresh path destroys the strip when the supported taskbar or notification area is unavailable; existing bounded Shell Recovery retries embedding without altering unrelated taskbar children.
