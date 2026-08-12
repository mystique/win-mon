Status: ready-for-agent

# Win Mon v1

## Problem Statement

On Windows 11, the user wants a glanceable live view of network upload and download rates on the taskbar (next to the notification area), without opening a dashboard or main window. Existing tools are heavy (skins, sensors, multi-OS paths). They need a minimal tray-driven utility: pick one NIC or all traffic, see two-line rates, exit cleanly.

## Solution

**Win Mon** is a Win11-only native tray utility. After launch it shows a **Tray Icon** (tooltip `Win Mon`) and embeds a display-only **Rate Strip** on the primary bottom taskbar: upload on top with ↑, download below with ↓, e.g. `1.1K/s`. The **Operator Menu** (tray only) offers radio **NIC Selection** — **All** plus each **NIC** — and **Exit**. No main window, no settings file, no autostart. MFC is a thin host only.

## User Stories

1. As a Win11 user, I want to run Win Mon and see a Tray Icon named Win Mon, so that I know the utility is active without a main window.
2. As a Win11 user, I want a two-line Rate Strip on the primary bottom taskbar, so that I can read upload and download rates at a glance.
3. As a Win11 user, I want upload on the top line with ↑ and download on the bottom line with ↓, so that direction is obvious.
4. As a Win11 user, I want rates formatted like `0.0K/s`, `1.1K/s`, or `12.3M/s` (and G when needed), so that values stay short and readable.
5. As a Win11 user, I want units to auto-scale among K/s, M/s, and G/s using base 1000, so that I do not think in raw bytes.
6. As a Win11 user, I want values below 1K/s still shown as `x.xK/s` (never bare bytes or a `B` in the unit), so that width and style stay consistent.
7. As a Win11 user, I want exactly one decimal place on the Rate Strip, so that numbers do not jump in layout noise.
8. As a Win11 user, I want Rate Samples about once per second, so that the strip feels live without frantic flicker.
9. As a Win11 user, I want the Rate Strip width to stay stable (sized for a wide sample such as `999.9G/s`), so that neighboring taskbar chrome does not jitter.
10. As a Win11 user, I want the Rate Strip to use a system UI font, so that it matches the shell.
11. As a Win11 user, I want Rate Strip text color to follow the taskbar/system theme when practical, so that light and dark taskbars stay readable.
12. As a Win11 user, I want the Rate Strip to be display-only (no click, menu, or tooltip), so that I do not hit invisible controls on the taskbar.
13. As a Win11 user, I want all control via the Tray Icon Operator Menu only, so that there is a single operator entry.
14. As a Win11 user, I want the Operator Menu to list **All** first, then each NIC in system order, then a separator and **Exit**, so that navigation is predictable.
15. As a Win11 user, I want English labels only on the Operator Menu, so that copy matches the product language.
16. As a Win11 user, I want radio marks on the current NIC Selection, so that I see what is selected.
17. As a Win11 user, I want **All** selected by default every launch, so that I see aggregate traffic immediately.
18. As a Win11 user, I want **All NICs** to mean the sum of rates over NICs that are up, excluding loopback, so that idle/down interfaces do not dilute the total.
19. As a Win11 user, I want to select a single NIC and see only that NIC’s Upload Rate and Download Rate, so that I can isolate Wi‑Fi vs Ethernet (etc.).
20. As a Win11 user, I want every non-loopback NIC listed whether up or down, so that the menu does not flap as links drop.
21. As a Win11 user, I want NIC labels to prefer friendly/alias names and fall back to description, so that names are human-readable.
22. As a Win11 user, when my selected NIC is still listed but down, I want zeros on the Rate Strip and the selection kept, so that my choice is not silently changed.
23. As a Win11 user, when my selected NIC disappears, I want NIC Selection to fall back to All NICs, so that the strip keeps showing something useful.
24. As a Win11 user, when nothing is up under All NICs (or there are no NICs), I want both lines at `0.0K/s`, so that empty network state is obvious.
25. As a Win11 user, I want no persistence of NIC Selection or other settings, so that each run starts from the same defaults.
26. As a Win11 user, I want no autostart registration by Win Mon, so that install surface stays minimal.
27. As a Win11 user, I want Single Instance behavior: a second launch exits silently, so that I never get two strips or two tray icons.
28. As a Win11 user, I want Exit to remove the Tray Icon, destroy the Rate Strip, and end the process with no ghost icons, so that leaving is clean.
29. As a Win11 user, I want Exit never to resize or permanently alter other taskbar children, so that the shell is not damaged.
30. As a Win11 user, if the Tray Icon cannot be created at startup, I want an error message and process exit, so that a broken run is not silent.
31. As a Win11 user, if the Rate Strip cannot embed but the Tray Icon works, I want the process to stay up and retry, so that I can still use the Operator Menu and recover when the shell is ready.
32. As a Win11 user, after Explorer/taskbar recreation, I want Shell Recovery to restore Tray Icon and Rate Strip without a prompt, keeping in-memory NIC Selection, so that shell restarts are survivable.
33. As a Win11 user, when DPI or the primary monitor changes, I want the Rate Strip re-found and relaid out, so that the strip stays correct.
34. As a Win11 user, when the primary taskbar is not bottom-aligned (or missing), I want the Rate Strip hidden while the Tray Icon remains, so that unsupported layouts do not mis-draw.
35. As a Win11 user, I want rates only on the primary taskbar, not secondary taskbars, so that multi-monitor chrome stays simple.
36. As a Win11 user, I want to run Win Mon as a normal user without elevation, so that I am not prompted for admin.
37. As a Win11 user, I accept that another taskbar embedder may overlap Win Mon, so that v1 need not coordinate multi-app layout.
38. As a Win11 user, I want no main window at any time, so that the product stays tray-only.
39. As a developer agent, I want MFC limited to a thin host (app object, message sink, strip window, tray/menu, timers), so that unrelated MFC features are not pulled in.
40. As a developer agent, I want Rate Strip embedding to follow the Win11 Shell_TrayWnd child approach (notification-area anchor), so that placement matches the intended locus without classic task-list surgery.
41. As a developer agent, I want a single pure logic seam (WinMonCore) for rates, formatting, selection, menu model, and strip visibility intent, so that behavior is testable without HWND/GDI.
42. As a developer agent, I want TrafficMonitor used only as a read-only reference for Rate Strip embed (and related sampling ideas), so that Win Mon stays a separate minimal codebase (ADR-0006).
43. As a developer agent, I want Win Mon’s solution and sources never to link, include, or vendor TrafficMonitor as a library, so that the product does not depend on that tree at build or run time.

## Implementation Decisions

### Architecture

- Greenfield product code under this repo; docs already define language (`CONTEXT.md`) and ADRs `0001`–`0006`.
- Two layers only:
  1. **WinMonCore** — pure logic, no MFC, no HWND (the sole automated test seam).
  2. **Shell host** — minimal MFC app that owns Tray Icon, Operator Menu, Rate Strip window, timers, and Win32/shell calls; translates OS events into core inputs and applies core outputs.
- No doc/view, ribbon, toolbar, property-sheet product UI, OLE, printing, MFC DB/sockets, HTML views, docking, app themes, recent-file lists, or registry-backed product settings (ADR-0003).
- No settings persistence and no autostart (ADR-0004).
- Normal user; no multi-embedder arbitration (ADR-0005).

### WinMonCore responsibilities

- Accept NIC snapshots: stable id, display name, up/down, byte counters (octets in/out), sample time.
- Exclude loopback from the NIC universe used for menu and aggregation (host may filter before core, or core filters — one place only, documented in core).
- Compute Upload Rate / Download Rate from consecutive Rate Samples (~1s): delta octets / delta seconds for the selected NIC, or sum of per-NIC rates for All NICs over NICs that are up.
- Format rates: base 1000; tiers K/M/G; always at least K; one decimal; suffix `/s`; no `B` in the unit; arrows are shell presentation (core may expose numeric rates + formatted magnitude strings; shell draws `↑`/`↓`).
- Maintain NIC Selection: default All each process start; select All or select by id; if selected id missing → All; if present but down → keep, rates zero.
- Expose Operator Menu model: ordered entries (All, NICs in given enum order), which id is checked, Exit as a command (not a NIC).
- Expose Rate Strip visibility intent: show only when environment says primary bottom taskbar available; otherwise hide (shell still may retry embed).
- Exit is a command flag/intent for the shell to tear down; core does not touch the OS.

### Shell host responsibilities

- Single Instance: second process exits silently (e.g. named mutex or equivalent — mechanism internal to host).
- Create Tray Icon with tip `Win Mon`; failure at startup → message box → exit process.
- Build Operator Menu from core menu model on each open (or on NIC list refresh); radio + Exit; English.
- Enumerate NICs via normal-user Windows APIs; refresh list on a sensible cadence or on menu open; feed core.
- Rate Sample timer ~1s; feed counters into core; invalidate Rate Strip.
- Rate Strip: child window embedded into primary `Shell_TrayWnd`, anchored near the notification area (TrafficMonitor Win11 path as reference); owner-drawn two lines; fixed width from wide sample; system font; theme-aware text color when practical (ADR-0001, ADR-0002).
- Never shrink or modify `MSTaskSwWClass` / task list windows.
- No interaction handlers on the Rate Strip (ignore click/context menu).
- Shell Recovery: detect explorer/taskbar recreation; recreate tray + strip; keep core state.
- On DPI / primary monitor change: re-find taskbar and relayout strip.
- Non-bottom or missing primary taskbar: hide strip, keep tray + sampling.
- Exit: `NIM_DELETE`, destroy strip, end process.
- Strip embed failure with tray OK: remain running, retry (timer and/or recovery path).

### Product defaults (every launch)

- NIC Selection = All NICs.
- No disk/registry product config read/write.
- English UI only.

### TrafficMonitor (reference only, ADR-0006)

- The optional `TrafficMonitor/` tree is **read-only reference**, especially for Win11 taskbar Rate Strip embed technique.
- **Allowed:** read TM sources; re-implement ideas in Win Mon–owned files; hand-adapt short snippets into new Win Mon sources (no shared build).
- **Forbidden:** add TM (or subprojects) to the Win Mon solution; link TM libs; `#include` paths under `TrafficMonitor/`; shared source filters; submodule-as-dependency; copy entire TM modules/skins/plugins into the product.
- Do not port skins, plugins, multi-OS taskbar paths, or settings UI.

## Testing Decisions

### What good tests look like

- Test **observable core behavior** only: given NIC snapshots, times, and commands → formatted rates, selection, menu model, visibility intent.
- Do **not** assert MFC message maps, HWND parenting, pixel buffers, or Win32 API call sequences in unit tests.
- Prefer table-driven cases at the WinMonCore boundary.
- Deterministic clocks/counters injected via snapshot data (no real sleep required).

### What is tested (automated)

- **WinMonCore only** (confirmed seam):
  - Format tiers and edges (`0.0K/s`, sub-kilo as K, K→M→G boundaries, one decimal, no `B`).
  - Single-NIC delta rates; zero when down; zero when first sample insufficient.
  - All NICs sums only up NICs; loopback never included.
  - Selection default All; select NIC; disappear → All; down but present → keep + zero.
  - Menu order: All first, NICs in input order, Exit available as command.
  - Visibility intent vs bottom-primary flag.

### What is not unit-tested (v1)

- Tray Icon add/delete, Operator Menu HWND, SetParent embed, Shell Recovery, DPI, Single Instance mutex, theme color picks.
- These are covered by **manual smoke** on Win11 after build: launch, strip locus, menu, NIC switch, Exit cleanup, optional explorer restart.

### Prior art

- None in this repo (docs-only greenfield). Do not couple tests to TrafficMonitor binaries.

## Out of Scope

- Main window, settings UI, skins, graphs, CPU/GPU/temperature, plugins.
- Win10 classic taskbar embedding; secondary taskbars; left/top/right Rate Strip layout.
- Autostart; any persistence of NIC Selection or options.
- Bits/s, KB/s-style units, configurable sample interval, horizontal one-line strip.
- Rate Strip as a control surface (click/menu/tooltip).
- Auto-select “busiest NIC”.
- Admin-only features; multi-app taskbar embed arbitration.
- Localization beyond English.
- Depending on or vendoring TrafficMonitor as a build/runtime dependency (ADR-0006); wholesale TM feature port.
- Unrelated MFC framework features (ADR-0003).

## Further Notes

- Glossary: root `CONTEXT.md`. Use those terms in code and tests.
- ADRs: `docs/adr/0001` embed; `0002` Win11 primary bottom; `0003` thin MFC; `0004` no persistence/autostart; `0005` normal user / no embed arbitration; `0006` TrafficMonitor reference-only.
- Issue tracker path: `.scratch/win-mon-v1/spec.md`.
- Implementation should not start from this file’s absence of paths: create a minimal MFC solution structure as needed, keeping WinMonCore isolatable for tests (e.g. static lib or same solution test project).
- Verification bar for an implementing agent: WinMonCore tests green + manual Win11 smoke of tray, strip, menu, Exit.
