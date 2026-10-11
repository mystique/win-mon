# Win Mon

A Win11-only tray utility named **Win Mon** that shows live NIC upload/download rates as a two-line strip on the primary taskbar and in an optional Floating Rate Display. UI copy is English only, and menu labels stay plain: internal terms like NIC and Rate Strip never appear in them. There is no main window. Operator settings persist in the registry (see ADR-0007), except Network to Monitor, which resets each launch. MFC is used only as a thin host for this surface (see ADR-0003), not as a general application framework feature set.

## Language

**Win Mon**:
The product and process the user runs. Tray tooltip and identity string are `Win Mon`.
_Avoid_: win-mon (except repo/folder), TrafficMonitor (reference tree only — ADR-0006), bandwidth monitor

**Rate Strip**:
The two-line Upload/Download Rate text embedded on the primary bottom taskbar only (see ADRs). Upload on top with ↑, download below with ↓. Speed Text Position chooses Left or Right (default Right): arrows precede the readings on Left and follow them on Right. Numeric readings are right-aligned in fixed columns; unit and arrow anchors remain stationary as rates change. It ignores the mouse unless **Right-Click Speed Text** is on, and even then its only response is opening the Operator Menu — no click command, no tooltip. Hidden when that taskbar is missing or not bottom-aligned; not shown on secondary taskbars. Width is stable (measured for four integer digits and one decimal using the widest active-font digits, units and arrows; oversized numeric readings show an ellipsis), starts with the DPI-aware Windows UI message font, and uses the persisted Rate Font when one has been selected; text color follows the same `AppsUseLightTheme` registry state as the Tray Icon. While running, Win Mon refreshes the strip position and theme color once per Rate Sample so changes in the notification area do not cause overlap or stale contrast. If embed fails while the Tray Icon is up, the process stays alive and retries (including via Shell Recovery). "Rate Strip" is an internal term: no menu label uses it.
_Avoid_: taskbar window, widget, HUD, overlay (unless contrasting implementation), main UI

**Floating Rate Display**:
An optional, borderless, always-on-top dark capsule, approximately 214x48 DIP with speed text shown. Green upload and blue download semicircles fill symmetrically from their left and right ends toward the top/bottom midpoint. Their outer diameter and each chart's clip width have an exact 5:8 ratio. Independent, aligned upper/lower continuous trends have faint grids, clipped gradient fills, a 1.5 DIP center gap, and 32 real Rate Samples sharing the recent peak scale (minimum 1000 bytes/s, smoothing 0.35). The inner traveling water wave uses activity-dependent 2200–1000ms periods and a 15%–80% target level; idle water drains to zero. A shared approximately 30 FPS native timer animates water and text visibility without adding samples; hidden and settled idle displays stop it, and disabled Windows animations use static endpoints.

Each readout uses nine fixed Punctum LED cells (arrow, three right-aligned integer cells, decimal point, fraction, three unit cells). The decimal cell has 21 round lamps (3x7); the other cells have 35 (5x7), including dim leading numeric cells; gaps have no lamps. Embedded OFL glyph masks render independently of Rate Font, without font installation. Values beyond the three-digit capacity saturate at 999.9 in the core-selected unit, leaving Rate Strip formatting unchanged. Show Floating Speed Text fades both complete readouts and their dim lamps while narrowing the actual capsule to ring and unchanged charts; showing restores width. Transitions use reversible ease-out cubic, 220ms hiding/180ms showing, and share the water timer. The current body and separate input-transparent shadow resize together, retaining fixed chart geometry and DPI scaling. The surface has no taskbar or Alt-Tab entry, drags without activation, and opens the Operator Menu on right-click. It remains available while the Rate Strip is hidden. Network changes reset graphical history/activity. Visibility, text visibility and desktop position persist; first use or an unusable restored position uses the primary work area's upper-right corner. Expansion constrains the changing geometry to the monitor work area.

_Avoid_: floating window, dashboard, overlay, main window

**Tray Icon**:
The notification-area icon that is the primary operator entry (Operator Menu only). Required for a successful run; if it cannot be created at startup, Win Mon shows an error message and exits.
_Avoid_: main window, shell icon (ambiguous)

**Operator Menu**:
The tray context menu, opened from the Tray Icon, from the Rate Strip when **Right-Click Speed Text** is on, or from the Floating Rate Display. Structure: **Network to Monitor** submenu, **Speed Text Position** submenu (Left / Right), separator, **Launch at Login**, **Right-Click Speed Text**, **Show Floating Display**, **Show Floating Speed Text**, separator, disabled `Font: <name>`, **Set Font...**, separator, **Exit**. The four toggles form one group with no separator between them; the font summary and action form the next group. The submenu is a radio list with **All** first, then each NIC represented in the classic Windows Network Connections folder (friendly/alias name, else description). Labels are plain English, free of internal terms such as NIC or Rate Strip.
_Avoid_: main menu, settings dialog

**Launch at Login**:
Operator Menu check item that registers or removes Win Mon's `HKCU\...\CurrentVersion\Run` entry pointing at the current executable. Reflects the live registry state each time the menu opens.
_Avoid_: autostart (as label), startup entry

**Right-Click Speed Text**:
Operator Menu check item that decides whether the Rate Strip answers a right-click by opening the Operator Menu. Off by default. Persisted as the `RightClickSpeedText` DWORD under `HKCU\Software\Win Mon` and applied at launch, so the choice survives a restart. While off, the strip stays display-only and clicks fall through to the taskbar. It does not affect the Floating Rate Display.
_Avoid_: enable clicks, interactive mode, hotspot

**Show Floating Display**:
Operator Menu check item that shows or hides the Floating Rate Display. Off by default and persisted as the `FloatingRateDisplay` DWORD under `HKCU\Software\Win Mon` across restarts.
_Avoid_: show window, floating display toggle


**Show Floating Speed Text**:
Operator Menu check item, on by default. Persists as the `FloatingRateTextVisible` DWORD (0/1; absent, unreadable, wrong type or unknown values mean on). Applies to both floating LED rows and dim lamps, with reversible width/opacity transitions; can be configured while the display is closed. Failed persistence restores the prior live choice.

**Rate Font**:
The font selection used by the Rate Strip; Floating Rate Display uses fixed embedded LED glyphs. With no saved choice, it uses the current DPI-aware Windows UI message font reported by `SPI_GETNONCLIENTMETRICS`. The Operator Menu shows the active face as a disabled `Font: <name>` item; **Set Font...** opens the standard Windows screen-font chooser (sizes 6–12 points, which fit the two-line taskbar surface) and applies the selected face, style, and size immediately. The accepted face, style, and size are stored as the versioned `RateFont` value under `HKCU\Software\Win Mon`, loaded at startup and re-read before Rate Strip recreation during relayout or Shell Recovery. Invalid or unreadable data falls back to the Windows UI font.
_Avoid_: text font, display font

**Exit**:
Operator Menu command that removes the Tray Icon, destroys the Rate Strip and Floating Rate Display, and ends the process without leaving shell chrome behind and without altering other taskbar children.
_Avoid_: Quit, Close

**NIC**:
A system network interface the user may select for rate display. Network to Monitor lists non-loopback NICs represented in the classic Windows Network Connections folder, whether up or down; if that classification is unavailable, it retains all enumerated non-loopback NICs. NIC stays an internal term: the menu says Network.
_Avoid_: connection, adapter (as domain term), interface (unless Windows API talk)

**All NICs**:
The default aggregate selection each launch: sum of rates over hardware NICs that are up, excluding loopback and NICs absent from the classic Windows Network Connections folder when that classification is available. If classification is unavailable, sum all enumerated up non-loopback hardware NICs. Virtual and tunnel NICs remain individually selectable but do not participate in All NICs. Menu label: `All`.
_Avoid_: Auto, total speed, select all, 全部

**Upload Rate** / **Download Rate**:
Bytes-per-second throughput shown by the Rate Strip and Floating Rate Display. Units auto-scale among `K/s`, `M/s`, and `G/s` (base 1000), always at least `K/s` (never bare bytes or a `B` in the unit), one decimal place — e.g. `0.0 K/s`, `1.1 K/s`, `12.3 M/s`.
_Avoid_: speed (alone), bandwidth, traffic, KB/s, MB/s, B/s

**Rate Sample**:
One periodic measurement used to compute Upload/Download Rate. Interval: 1 second.
_Avoid_: poll, tick (unless implementation)

**Network to Monitor**:
Menu label for which NIC (or All NICs) feeds both the Rate Strip and Floating Rate Display. Default each launch: All NICs. Not persisted. If the selected NIC disappears from the system, selection falls back to All NICs; if it remains but is down, keep selection and show zero rates. Under All NICs with nothing up (or no NICs), both rates show as zero.
_Avoid_: connection preference, remembered adapter

**Single Instance**:
At most one Win Mon process owns the Tray Icon, Rate Strip and Floating Rate Display. A second launch exits silently without feedback.
_Avoid_: mutex (implementation), singleton app

**Shell Recovery**:
After Explorer/taskbar recreation, Win Mon re-creates the Rate Strip and Tray Icon and keeps the in-memory Network to Monitor choice and sampling state. The message sink is a hidden top-level window so it receives the registered `TaskbarCreated` broadcast. No user prompt. DPI change and primary-monitor change re-find/relayout the Rate Strip while running; the periodic refresh also tracks notification-area geometry and theme color changes.
_Avoid_: restart app, relaunch

**Speed Text Position**:
The operator's persisted choice of the primary bottom taskbar's left-side or notification-side location for the Rate Strip, with Right as the default. Direction arrows sit on the outer side of the reading; stable numeric and unit columns keep rate changes from moving the unit or arrow anchors. Left temporarily hides when its region is occupied or the Windows shell geometry cannot be verified, retaining the selection and retrying each Rate Sample.
_Avoid_: alignment toggle, taskbar orientation

Extension (2026-10-11): Show Floating Chart independently controls both trend regions and persists as FloatingRateChartVisible (DWORD, default on). It uses the existing reversible 220ms hide/180ms show animation and shared timer; hiding shifts speed text left. Hiding both charts and text leaves a circular 48 DIP body with the original ring diameter, 54 DIP canvas including shadow. Sampling history continues while charts are hidden.
