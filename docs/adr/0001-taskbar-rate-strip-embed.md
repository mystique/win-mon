# Taskbar Rate Strip via Win11 shell embed

Win Mon must show two-line rates in the notification-side taskbar locus (not a free-floating dashboard). We embed a child window into the primary `Shell_TrayWnd` (TrafficMonitor Win11 path: parent to the tray shell window, anchor near the notification area), rather than a topmost overlay or undocumented taskbar pin APIs.

Overlay positioning breaks more easily under fullscreen, overview, and shell restarts; pin APIs are out of scope. Classic taskbar surgery (shrinking `MSTaskSwWClass`) is explicitly rejected — Win11-only, no permanent layout damage to the task list. If the primary taskbar is not bottom-aligned, hide the Rate Strip and keep the Tray Icon.
