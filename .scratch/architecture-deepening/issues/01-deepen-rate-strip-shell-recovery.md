# 01 — Deepen the Rate Strip / Shell Recovery lifecycle

**What to build:** Keep the Rate Strip correct through startup, Rate Sample refresh, shell loss, recreation, DPI/display/theme changes, transient shell failures, and Rate Font recreation while moving that lifecycle knowledge behind one deep module interface. The MFC message host should forward Windows signals without reconstructing visibility, child-window validity, relayout order, or retryability.

**Blocked by:** None — can start immediately.

**Status:** resolved

- [x] A supported primary bottom taskbar produces the Rate Strip; a missing or unsupported taskbar keeps it hidden without treating that state as a retryable failure.
- [x] Shell Recovery recreates the Tray Icon and Rate Strip without duplicates while retaining the in-memory Network to Monitor choice and Rate Sample state.
- [x] Transient Explorer/taskbar failures retry until the shell surface is restored, and shutdown cancels pending recovery work.
- [x] DPI, display, setting, and theme changes relayout or recreate the Rate Strip through the same lifecycle implementation used by startup and Shell Recovery.
- [x] Each Rate Sample still refreshes notification-area placement and theme-aware text color without exposing child-window state to the host.
- [x] The deep module interface is the lifecycle test surface; the weightless visibility predicate and tests of that pass-through are removed.
- [x] Deterministic lifecycle-transition checks and a Windows smoke scenario cover supported, hidden, lost-shell, retry, recovery, and shutdown outcomes.
