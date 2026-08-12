# 04 — Select one NIC from the Operator Menu

**What to build:** Make the Tray Icon Operator Menu list All and every non-loopback NIC as radio choices. Selecting one changes the live Rate Strip to that NIC; WinMonCore owns the selection and churn rules while the shell presents the current menu model.

**Blocked by:** 03 — Display live All NICs rates.

**Status:** resolved

- [ ] The Operator Menu lists All first, then every non-loopback NIC in Windows enumeration order, then a separator and Exit.
- [ ] NIC labels prefer the Windows friendly or alias name and fall back to description.
- [ ] Both up and down NICs remain visible in the menu.
- [ ] Exactly one radio item represents the current NIC Selection.
- [ ] All is selected by default every process launch.
- [ ] Selecting a NIC changes the live Rate Strip to that NIC’s Upload Rate and Download Rate.
- [ ] Selecting All changes the Rate Strip to the aggregate over up non-loopback NICs.
- [ ] A selected NIC that remains enumerated but goes down stays selected and displays zero rates.
- [ ] A selected NIC that disappears causes NIC Selection to fall back to All.
- [ ] When no NIC is available or none is up under All, both lines display `0.0K/s`.
- [ ] NIC appearance, disappearance, or state change is reflected without restarting Win Mon.
- [ ] Selection is in memory only and resets to All after process restart.
- [ ] The Rate Strip remains display-only; the Operator Menu opens only from the Tray Icon.
- [ ] Automated WinMonCore tests cover default selection, select-one, select-All, menu input order, checked item, down-but-present, disappeared selection, and empty NIC sets.
- [ ] Manual Windows 11 smoke verifies menu labels, radio behavior, All aggregation, single-NIC display, and in-session NIC churn where available.
