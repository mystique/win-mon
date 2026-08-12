# 06 — Verify the complete Win Mon v1 contract

**What to build:** Produce and verify the complete release-ready Win Mon v1 executable as one product. This ticket adds no product behavior; it closes integration gaps and proves the assembled app satisfies the approved spec, glossary, and ADRs.

**Blocked by:** 01 — Launch Win Mon from the Tray Icon; 02 — Embed a static Rate Strip; 03 — Display live All NICs rates; 04 — Select one NIC from the Operator Menu; 05 — Recover from Windows shell and display changes.

**Status:** resolved

- [ ] A clean release build succeeds for the intended Windows 11 architecture and documented Visual Studio/MFC toolchain.
- [ ] All WinMonCore automated tests pass from a clean workspace.
- [ ] Manual Windows 11 smoke verifies launch, Tray Icon tooltip, live All NICs rates, one-NIC selection, zero states, and Exit.
- [ ] Manual smoke verifies a second launch exits silently without duplicate product surfaces.
- [ ] Manual smoke verifies Explorer restart recovery and preserved in-memory NIC Selection.
- [ ] Manual smoke verifies primary-taskbar/DPI behavior available on the test workstation.
- [ ] Exit leaves no Tray Icon ghost, Rate Strip, process, or taskbar layout damage.
- [ ] No main window or Rate Strip interaction is exposed.
- [ ] No product setting is persisted and no autostart registration is created.
- [ ] Win Mon runs without administrator elevation.
- [ ] No Rate Strip is shown on a secondary or unsupported non-bottom taskbar.
- [ ] The MFC dependency and product-surface audit finds no feature unrelated to the Tray Icon, Operator Menu, Rate Strip, Rate Samples, NIC Selection, Single Instance, Shell Recovery, and Exit.
- [ ] The build dependency audit finds no TrafficMonitor project, library, include path, shared source, vendored module, or runtime artifact.
- [ ] Only Win Mon-owned product code and required platform/MFC runtime artifacts are needed to build and run Win Mon.
- [ ] Any acceptance gap discovered by integration is fixed and reverified before this ticket is complete.
