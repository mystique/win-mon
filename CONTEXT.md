# Win Mon

A Win11-only tray utility named **Win Mon** that shows live NIC upload/download rates as a two-line strip on the primary taskbar. UI copy is English only. There is no main window, no autostart, and no saved settings. MFC is used only as a thin host for this surface (see ADR-0003), not as a general application framework feature set.

## Language

**Win Mon**:
The product and process the user runs. Tray tooltip and identity string are `Win Mon`.
_Avoid_: win-mon (except repo/folder), TrafficMonitor (reference tree only — ADR-0006), bandwidth monitor

**Rate Strip**:
The two-line Upload/Download Rate text embedded on the primary bottom taskbar only (see ADRs). Upload on top with ↑, download below with ↓. Display-only: not a control surface (no click, menu, or tooltip). Hidden when that taskbar is missing or not bottom-aligned; not shown on secondary taskbars. Width is stable (sized for a wide sample such as `999.9G/s`), system UI font, text color follows taskbar/system theme when practical. If embed fails while the Tray Icon is up, the process stays alive and retries (including via Shell Recovery).
_Avoid_: taskbar window, widget, HUD, overlay (unless contrasting implementation), main UI

**Tray Icon**:
The notification-area icon that is the sole operator entry (Operator Menu only). Required for a successful run; if it cannot be created at startup, Win Mon shows an error message and exits.
_Avoid_: main window, shell icon (ambiguous)

**Operator Menu**:
The tray context menu opened only from the Tray Icon. Structure: radio list with **All** first, then each NIC in system enumeration order (friendly/alias name, else description), separator, **Exit**. English labels only.
_Avoid_: main menu, settings dialog

**Exit**:
Operator Menu command that removes the Tray Icon, destroys the Rate Strip, and ends the process without leaving shell chrome behind and without altering other taskbar children.
_Avoid_: Quit, Close

**NIC**:
A system network interface the user may select for rate display. Menu lists every non-loopback NIC (up or down).
_Avoid_: connection, adapter (as domain term), interface (unless Windows API talk)

**All NICs**:
The default aggregate selection each launch: sum of rates over NICs that are up, excluding loopback. Menu label: `All`.
_Avoid_: Auto, total speed, select all, 全部

**Upload Rate** / **Download Rate**:
Bytes-per-second throughput on the Rate Strip. Units auto-scale among `K/s`, `M/s`, and `G/s` (base 1000), always at least `K/s` (never bare bytes or a `B` in the unit), one decimal place — e.g. `0.0K/s`, `1.1K/s`, `12.3M/s`.
_Avoid_: speed (alone), bandwidth, traffic, KB/s, MB/s, B/s

**Rate Sample**:
One periodic measurement used to compute Upload/Download Rate. Interval: 1 second.
_Avoid_: poll, tick (unless implementation)

**NIC Selection**:
Which NIC (or All NICs) feeds the Rate Strip. Default each launch: All NICs. Not persisted. If the selected NIC disappears from the system, selection falls back to All NICs; if it remains but is down, keep selection and show zero rates. Under All NICs with nothing up (or no NICs), both rates show as zero.
_Avoid_: connection preference, remembered adapter

**Single Instance**:
At most one Win Mon process owns the Tray Icon and Rate Strip. A second launch exits silently without feedback.
_Avoid_: mutex (implementation), singleton app

**Shell Recovery**:
After Explorer/taskbar recreation, Win Mon re-creates the Rate Strip and Tray Icon and keeps the in-memory NIC Selection and sampling state. No user prompt. DPI change and primary-monitor change also re-find/relayout the Rate Strip while running.
_Avoid_: restart app, relaunch
