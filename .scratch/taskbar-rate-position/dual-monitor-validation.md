# Dual-monitor taskbar diagnosis and validation

Date: 2026-10-12

## Environment and reproduction

Windows 11 build 22000, two 1920x1080 displays side by side, primary on the left. Primary taskbar: `(0,1032)-(1920,1080)`, 96 DPI. Right-Click Speed Text was enabled. The previously running application was closed normally for rebuilding, and its registry preferences were restored after smoke testing.

The previous native test skipped visible Left checks on this desktop. A stricter invocation reproduces the disappearing-text path by creating Right, selecting Left and requiring visible Left:

```powershell
build/Release/RateStripWindowTests.exe build/rate-strip-evidence --require-left
```

Before the occupancy fix, exit code 1: `unoccupied Left must remain visible after switching from Right`. A targeted probe identified `ReBarWindow32`, spanning `(0,1032)-(1655,1080)`, as the rejecting native window. This is a layout container; its actual task-list child occupies only the center. The build-22000 XAML host also exposes StartButton without the newer TaskbarFrameAutomationPeer, so requiring that frame rejected otherwise free space.

## Changes

- Ignore only the recognized ReBar layout container containing MSTaskSwWClass; still enumerate and check its children. Keep checking all visible XAML controls, including complete Start-button padding. Accept a verified XAML host with StartButton on the older shell without requiring a frame peer.
- Convert the Rate Strip from WS_POPUP/WS_SYSMENU to WS_CHILD before SetParent. Microsoft documents that SetParent does not change these styles: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent . The previous style fails the new embedded-child assertion.
- Native tests check taskbar-relative vertical centering, real WindowFromPoint/SendInput right-click delivery after switching and recreation, and input pass-through when the setting is off. Smoke now opens the actual application menu with mouse input, selects Left, then verifies visible Left, centering and the actual menu after an application restart. Both scripts can require usable Left instead of silently skipping it.

## Results and limits

CMake x64 Release configuration/build succeeded; all eight CTest tests passed. The strict native invocation passed with `visible Left verified=1`. `tests/windows-smoke.ps1 -RequireLeft` passed, including visible persisted Left on two launch fixtures. The new menu-open check waits for the previous menu to close before injecting the next click. Native screenshot `build/rate-strip-evidence/left.bmp` was inspected: both rows, arrows and readings are visible.

Right-click failure and the slight upward shift were not independently reproduced before the change in this desktop configuration; the popup/child mismatch is a confirmed integration defect, not proof of their sole cause. After the change, real right clicks open the menu after switching and restart, and window geometry is exactly vertically centered. Physical DPI changes, primary-monitor reassignment, newer shell builds and optical font alignment beyond the captured default font remain untested in this run. No display settings were changed.
