# MFC as a thin shell only

Win Mon uses MFC for the application object, windows, tray message sink, and menus — not as a full desktop-app framework surface.

**In scope:** minimal `CWinApp` (or equivalent), hidden top-level broadcast-capable message host as needed, `CWnd` (or thin owner-drawn child) for the Rate Strip, tray icon + context menu wiring, timers/messages required for Rate Sample, dynamic strip refresh, and Shell Recovery.

**Out of scope unless a product decision requires them:** doc/view or SDI/MDI, ribbons, toolbars/status bars, property sheets as product UI, OLE/ActiveX, printing, MFC sockets/database, WinInet, HTML views, docking, command-routing feature packs, app look themes, recent-file lists, registry-backed app profile state for product settings, and any other MFC facility that does not directly serve Rate Strip, Tray Icon, Operator Menu, Network to Monitor, sampling, Single Instance, Shell Recovery, or Exit.

MFC remains a thin host for the tray utility and must not pull in unrelated product chrome or dependencies. TrafficMonitor is useful only as a reference for shell embedding (ADR-0006).
