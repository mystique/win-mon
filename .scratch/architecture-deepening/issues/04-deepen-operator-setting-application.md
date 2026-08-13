# 04 — Deepen persisted operator-setting application

**What to build:** Make Launch at Login, Right-Click Speed Text, and Rate Font changes behave as complete operator-visible transactions: validate, persist, apply live, and handle failure without leaving the running state and registry state inconsistent. Preserve the distinct registry locations and defaults required by the ADRs while moving transaction semantics behind deep module interfaces.

**Blocked by:** 01 — Deepen the Rate Strip / Shell Recovery lifecycle; 03 — Deepen the Operator Menu transaction.

**Status:** resolved

- [x] A successful Right-Click Speed Text or Rate Font change applies immediately and survives process restart and Shell Recovery.
- [x] A failed persistence or live-application step leaves the previous operator-visible and persisted choice intact rather than producing divergent state.
- [x] Invalid, absent, unsupported-version, or unreadable Rate Font data falls back to the current DPI-aware Windows UI message font.
- [x] Launch at Login continues to use and reflect the live current-user Run entry each time the Operator Menu opens; a failed change is visible as unchanged live state.
- [x] Network to Monitor remains session-only and is never included in persisted operator settings.
- [x] Rate Sample performs no registry reads; persisted Rate Font is read only when startup or Rate Strip recreation requires it.
- [x] Deterministic tests at the deep module interfaces cover absent/corrupt defaults, storage failure, live-application failure, rollback, restart, and recreation consistency without exposing a hypothetical public adapter seam.
- [x] A Windows smoke scenario confirms the real registry and Rate Strip implementations preserve immediate application and restart behavior.
