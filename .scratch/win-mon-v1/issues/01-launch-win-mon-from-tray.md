# 01 — Launch Win Mon from the Tray Icon

**What to build:** A minimal native MFC Win Mon process that launches without a main window, creates the Tray Icon as the sole operator entry, exposes an Operator Menu containing All and Exit, enforces Single Instance, and exits cleanly. MFC is a thin host only; TrafficMonitor is reference-only and must not become a build or runtime dependency.

**Blocked by:** None — can start immediately.

**Status:** resolved

- [ ] A normal user can launch Win Mon without elevation or a visible main window.
- [ ] Launch creates exactly one Tray Icon with tooltip `Win Mon`.
- [ ] Right-clicking the Tray Icon opens an English Operator Menu containing All, a separator, and Exit.
- [ ] A second launch exits silently and does not create another process-owned Tray Icon or other product surface.
- [ ] Selecting Exit removes the Tray Icon and terminates the process without a ghost icon.
- [ ] Failure to create the Tray Icon at startup shows an error message and terminates the process.
- [ ] Every launch starts with All selected; no product settings are read from or written to disk or the registry.
- [ ] Win Mon does not register autostart.
- [ ] The MFC surface is limited to facilities directly required for the application object, hidden message sink, Tray Icon, Operator Menu, Single Instance, and Exit.
- [ ] The solution does not link, include, vendor, or otherwise depend on TrafficMonitor projects, libraries, or sources.
- [ ] A clean build succeeds for the intended Windows 11 target.
- [ ] Manual smoke verifies launch, tooltip, Operator Menu, silent second launch, and clean Exit.
