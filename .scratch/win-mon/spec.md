# Win Mon

## Product

Win Mon is a Windows 11 tray utility that shows live upload and download rates in a two-line Rate Strip beside the primary taskbar notification area. It has no main window. The Tray Icon and, when enabled, the Rate Strip open the Operator Menu.

The Rate Strip shows upload above download, uses arrows for direction, formats bytes per second with base-1000 `K/s`, `M/s`, and `G/s` units, and updates once per second. Its width remains stable for a wide sample such as `999.9G/s`.

## Operator Menu

The Operator Menu contains:

1. **Network to Monitor**: **All** followed by the non-loopback NICs represented in the classic Windows Network Connections folder.
2. **Launch at Login**: controls Win Mon's `HKCU\...\CurrentVersion\Run` entry.
3. **Right-Click Speed Text**: controls whether right-clicking the Rate Strip opens the Operator Menu.
4. `Font: <name>` and **Set Font...**: report and select the Rate Font.
5. **Exit**: removes Win Mon's shell surfaces and terminates the process.

Network to Monitor starts at All NICs on every launch. A selected NIC that remains available but is down stays selected and reports zero rates; a NIC that disappears causes selection to return to All NICs.

Launch at Login, Right-Click Speed Text, and Rate Font persist as user-scoped registry settings. Missing or invalid persisted data uses the documented defaults.

## Rate Strip

- Embedded as a child of the primary bottom `Shell_TrayWnd`; never shown on secondary or non-bottom taskbars.
- Right-aligned two-line text with upload on top and download below.
- Uses the current DPI-aware Windows UI message font unless a Rate Font is saved.
- Uses taskbar/system-theme-aware text color and a transparent layered surface.
- Refreshes placement and theme color during each Rate Sample.
- Ignores mouse input unless Right-Click Speed Text is enabled.
- Does not resize or permanently alter unrelated taskbar children.

## Network rates

- All NICs aggregates non-loopback NICs that are up.
- A single selected NIC reports only that NIC.
- The first or incomplete sample, unavailable counters, counter resets, and empty network states report zero rather than fabricated rates.
- Formatting always uses one decimal place and at least `K/s`; units never contain `B`.

## Runtime behavior

- Win Mon runs as a normal user and enforces Single Instance.
- Tray Icon creation failure shows an error and terminates startup.
- Rate Strip embedding failure leaves the Tray Icon operational and is retried.
- Explorer/taskbar recreation restores the Tray Icon and Rate Strip without prompting.
- DPI and primary-monitor changes re-find and relayout the supported Rate Strip.
- Exit removes the Tray Icon, destroys the Rate Strip, and leaves no Win Mon process or taskbar layout change.

## Architecture

- `WinMonCore` owns rate calculation, formatting, Network to Monitor state, Operator Menu models, and Rate Strip visibility intent without MFC or HWND dependencies.
- `WinMon` is a thin MFC shell host for the application object, hidden broadcast-capable message sink, Tray Icon, Operator Menu, Rate Strip, timers, settings, and Win32 integration.
- TrafficMonitor is reference-only and is never linked, included, vendored, or required at build or runtime.

## Verification

- WinMonCore automated tests cover rate boundaries, counter transitions, aggregation, selection, menu models, and visibility intent.
- Windows 11 smoke verification covers the actual Tray Icon, Operator Menu, Rate Strip rendering and placement, font selection, persisted operator settings, shell recovery, DPI handling, Single Instance, and clean Exit.

## Out of scope

- Main windows, dashboards, graphs, skins, plugins, and non-network sensors.
- Windows 10 taskbar paths, secondary taskbars, and vertical or top taskbar layouts.
- Persisting Network to Monitor.
- Localization beyond English.
- Administrator-only features or arbitration with other taskbar embedders.
- TrafficMonitor as a source, build, or runtime dependency.

`CONTEXT.md` defines the project vocabulary. `docs/adr/` defines the active design decisions.
