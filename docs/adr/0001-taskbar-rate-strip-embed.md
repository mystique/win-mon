# Taskbar Rate Strip via Win11 shell embed

The Rate Strip shows two-line rates in the notification-side taskbar locus. We embed a child window into the primary `Shell_TrayWnd` (TrafficMonitor Win11 path: parent to the tray shell window, anchor near the notification area), rather than a topmost overlay or undocumented taskbar pin APIs. The strip is right-aligned and re-reads the notification-area geometry during each one-second Rate Sample so a changing tray width cannot overlap the strip. The independent Floating Rate Display is governed by ADR-0008.

Overlay positioning breaks more easily under fullscreen, overview, and shell restarts; pin APIs are out of scope. Classic taskbar surgery (shrinking `MSTaskSwWClass`) is explicitly rejected — Win11-only, no permanent layout damage to the task list. If the primary taskbar is not bottom-aligned, hide the Rate Strip and keep the Tray Icon. The Tray Icon variant and Rate Strip text color use the same `AppsUseLightTheme` registry state, with dark mode as the fallback when that state is unavailable.

Shell recovery uses a hidden top-level message sink because the registered `TaskbarCreated` notification is a system broadcast and is not delivered to message-only windows.
