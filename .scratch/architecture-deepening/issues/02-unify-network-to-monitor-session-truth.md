# 02 — Unify Network to Monitor around one session truth

**What to build:** Make Rate Sample calculation and Operator Menu presentation consume one coherent view of the currently observable NICs and the session-only Network to Monitor choice. Windows enumeration, classic Network Connections eligibility, stable identity, selection reconciliation, and naming fallback should concentrate behind one deep module interface rather than producing purpose-specific observations.

**Blocked by:** None — can start immediately.

**Status:** resolved

- [x] Rate Sample and Operator Menu behavior derive from the same canonical NIC observation and selection state rather than separate menu-classified and sampling-only variants.
- [x] All NICs remains the default on each launch and aggregates only up, non-loopback NICs.
- [x] An available but down selected NIC remains selected and reports zero Upload Rate and Download Rate.
- [x] A selected NIC that disappears from the system falls back to All NICs before either rates or the Operator Menu are presented.
- [x] Network to Monitor lists non-loopback NICs represented in classic Network Connections, retaining all enumerated non-loopback NICs when that classification is unavailable.
- [x] Stable identity and friendly-name/description fallback knowledge no longer leaks separately into Rate Sample and Operator Menu callers.
- [x] Scenario tests observe both rate and menu outcomes across startup, selection, down/up changes, disappearance, counter churn, and classification-unavailable fallback through the deep module interface.
