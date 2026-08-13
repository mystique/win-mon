# 03 — Deepen the Operator Menu transaction

**What to build:** Preserve the documented Operator Menu from both the Tray Icon and Right-Click Speed Text while concentrating menu policy, command correlation, and chosen-command meaning behind one deep module interface. MFC should remain the thin drawing and tracking implementation rather than sharing a presentation-kind protocol, dynamic command side table, and separate dispatch protocol with domain logic.

**Blocked by:** 02 — Unify Network to Monitor around one session truth.

**Status:** resolved

- [x] Both Operator Menu entry points present the documented groups, labels, radio selection, check marks, Rate Font summary, and Exit action.
- [x] Every Network to Monitor choice maps to the intended session selection without a caller-maintained parallel NIC identifier table.
- [x] Launch at Login, Right-Click Speed Text, Set Font, and Exit choices reach their intended observable outcomes through the same command transaction.
- [x] Opening or tracking the Operator Menu cannot nest a second menu or invalidate command correlation during the first transaction.
- [x] Adding or changing one menu action no longer requires synchronized edits to a presentation-kind model, resource/dynamic identifier mapping, side table, and separate dispatcher.
- [x] Tests cross the deep module interface and assert command outcomes, grouping, copy, and selection semantics rather than vector sizes or positional item indices.
- [x] A focused Windows smoke scenario confirms the MFC drawing/tracking implementation executes a returned choice through the same transaction used by tests.
