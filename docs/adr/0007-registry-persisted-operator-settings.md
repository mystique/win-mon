# Operator settings persist under HKCU\Software\Win Mon

Start at Sign-in, Enable Taskbar Context Menu, Show Floating Display, the Floating Rate Display position, and Taskbar Display Position survive a restart. The product key is `HKEY_CURRENT_USER\Software\Win Mon`; the two display toggles are DWORD values (`TaskbarContextMenuEnabled` and `FloatingDisplayVisible`, 0 or 1, absent means off), and the floating display stores its absolute desktop position as `FloatingDisplayPosition`. Start at Sign-in uses its own `HKCU\...\CurrentVersion\Run` entry. Monitored Network remains session-only as defined by ADR-0004.

A setting the user must re-set at each logon is worse than no setting, and these settings are user-scoped registry state, so no install surface or elevation is added. HKCU keeps the write path available to a normal user (ADR-0005) and a read failure degrades to the documented default rather than blocking startup. The registry rather than an INI file avoids picking a config location and keeps the process free of file I/O on the startup path. Revisit if settings grow beyond a handful of flags, where a single serialized blob would read better than one value per flag.

Extension (2026-10-11; [spec](../../.scratch/taskbar-rate-position/spec.md)): persist Taskbar Display Position as the `TaskbarDisplayPosition` DWORD, 0 for Right and 1 for Left. Missing, unreadable, wrong-type or unknown values use Right; a setting change follows the existing live-apply, persistence and rollback transaction.

Extension (2026-10-11; [spec](../../.scratch/floating-led-display/spec.md)): `FloatingTextVisible` is a user DWORD, 0 hides both floating LED readouts and 1 shows them. Missing, unreadable, wrong-type and unknown values default to shown. The live-apply, save and rollback transaction also covers this setting, including when the floating display is closed. Persist only the preference and effective desktop position, never intermediate animation progress.

Extension (2026-10-11): FloatingChartsVisible independently persists chart visibility, default on, using the same live-apply/save/rollback transaction as floating text visibility.

Extension (2026-10-11): FloatingDisplayOpacity stores an integer 10–100 percentage as a DWORD, default 100 for missing, unreadable, wrong-type or out-of-range values. The native Floating Display Opacity submenu offers ten 10% steps. Apply and save on selection; restore the original value on save failure. Legacy slider values round to the nearest 10% on reading.

Menu terminology update (2026-10-14): use TaskbarContextMenuEnabled, TaskbarDisplayPosition, FloatingDisplayVisible, FloatingTextVisible, FloatingChartsVisible, FloatingDisplayOpacity and FloatingDisplayPosition. Old registry names are not migrated. The Run entry retains the product name Win Mon. Rate Font is no longer configurable; both displays use embedded LED glyphs.
