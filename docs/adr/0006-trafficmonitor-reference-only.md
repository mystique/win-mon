# TrafficMonitor is reference-only

The tree may contain a `TrafficMonitor/` checkout for humans and agents to **read**. Win Mon must not treat it as a product dependency.

**Allowed:** study Win11 taskbar Rate Strip embed and related sampling ideas; re-implement in Win Mon’s own sources (including adapting short snippets by hand into new files under the Win Mon project).

**Forbidden:** add TrafficMonitor (or its subprojects) to the Win Mon solution/build as a linked project, static/import lib, shared sources filter, `#include` of paths under `TrafficMonitor/`, submodule-as-library, or copy entire TM modules/skins/plugins into the product. Ship only Win Mon-owned code.

Rationale: Win Mon is a minimal tray utility (ADR-0003). TrafficMonitor is a large multi-feature app; coupling build or tree ownership would pull unrelated surface and license/maintenance weight. Reference is enough for the embed locus.
