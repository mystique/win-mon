# 03 — Display live All NICs rates

**What to build:** Replace the static Rate Strip values with live Upload Rate and Download Rate for All NICs. Introduce the single pure WinMonCore seam so real Windows NIC counters flow through deterministic rate and formatting logic to the visible taskbar display once per second.

**Blocked by:** 02 — Embed a static Rate Strip.

**Status:** ready-for-agent

- [ ] Win Mon enumerates non-loopback NICs using normal-user Windows APIs.
- [ ] A Rate Sample is taken about once per second and uses actual elapsed time when converting byte-counter deltas into bytes per second.
- [ ] All NICs aggregates only NICs that are currently up and always excludes loopback.
- [ ] The first sample, an incomplete sample, no NICs, or no up NICs produces zero Upload Rate and Download Rate rather than fabricated traffic.
- [ ] Counter resets, replacement, or invalid backward deltas do not produce an enormous or negative displayed rate.
- [ ] Upload Rate and Download Rate update the Rate Strip without moving or resizing the strip.
- [ ] Formatting uses base 1000, exactly one decimal place, and automatic K/s, M/s, or G/s tiers.
- [ ] Values below 1000 bytes per second remain in K/s; no display string contains `B`.
- [ ] Upload remains the top line with ↑ and download remains the bottom line with ↓.
- [ ] WinMonCore has no MFC, HWND, timer, or live Windows API dependency.
- [ ] Automated WinMonCore tests cover first-sample zero, single and multiple NIC deltas, up/down aggregation, loopback exclusion, zero state, counter reset, actual elapsed time, rounding, and K/M/G boundaries.
- [ ] Automated tests assert observable rates and display strings rather than internal classes or call sequences.
- [ ] Manual Windows 11 smoke confirms visible values change while producing real upload and download traffic.
- [ ] The implementation is Win Mon-owned; TrafficMonitor remains reference-only and is not linked, included, or vendored.
