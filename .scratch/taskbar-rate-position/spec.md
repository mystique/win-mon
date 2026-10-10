# Taskbar Speed Text Position

Status: ready-for-agent
Date: 2026-10-11
Design status: proposed; implementation has not started.

## Problem Statement

用户只能在主屏任务栏通知区侧查看 Rate Strip，希望可切换到任务栏左端。不断变化的数值长度不能带动单位和方向箭头移动；选择应像其他设置一样重启后保留，且更换字体不能破坏预留空间。

## Solution

Operator Menu 新增英文一级 `Speed Text Position`，悬停弹出 `Left` / `Right`，单选圆点标识当前选择。菜单位于 Network to Monitor 之后、已有分隔符之前，所有菜单入口一致。默认 Right，成功选择即时应用并保存。

Right 沿用通知区左邻位置，每行从左到右为数字、单位、箭头。Left 位于主屏底部任务栏左端，从左到右为箭头、数字、单位。上传在上、下载在下；数字靠单位右对齐，单位和箭头的栏位固定，窗口不随每次速率变化伸缩。字体或 DPI 改变后按实际字形重算。

“999.9 最大值”不能作为当前格式的保证：现有输出包括 `1000.0 M/s`，G/s 无上限。本设计保留共享格式规则，正常预留四位整数和一位小数；极端超宽数字以明确省略号代替，仅作用于 Rate Strip，单位和箭头完整保留。

## User Stories

1. As an operator, I want Right to be the initial position, so that an upgrade preserves the familiar taskbar location.
2. As an operator, I want a top-level position submenu, so that I can find both placement options together.
3. As an operator, I want exactly one of Left and Right marked, so that I can see the active choice.
4. As an operator, I want Left to move the reading to the taskbar's left end, so that I can read it in my preferred location.
5. As an operator, I want Right to restore the notification-side location, so that I can switch back easily.
6. As an operator, I want arrows before readings on Left and after readings on Right, so that direction remains clear at either end.
7. As an operator, I want upload above download, so that changing position does not change the familiar reading order.
8. As an operator, I want units and arrows to remain still as numeric lengths change, so that readings do not jump every second.
9. As an operator, I want both rows to share column anchors, so that the two readings remain aligned.
10. As an operator, I want K/s, M/s and G/s changes to keep the same columns, so that unit transitions do not move the strip.
11. As an operator, I want font and style changes to resize reserved space correctly, so that wide fonts remain legible.
12. As an operator, I want DPI changes to recalculate layout, so that scaling does not clip text or alter spacing unexpectedly.
13. As an operator, I want the selected position to survive application and Explorer restarts, so that I need not reset it.
14. As an operator, I want invalid saved settings to use Right safely, so that broken configuration does not stop startup.
15. As an operator, I want a failed setting change to restore the previous choice, so that displayed and saved state agree.
16. As an operator, I want the same menu from the tray, enabled taskbar right-click and floating display, so that configuration is consistent.
17. As an operator, I want system taskbar controls to remain accessible, so that the reading does not cover Widgets, Start or application icons.
18. As an operator, I want my Left choice retained when its area is temporarily unavailable, so that it returns when space becomes usable.
19. As an operator, I want the floating display to keep its own layout and position, so that this setting changes only the taskbar reading.
20. As an operator, I want an explicit overflow indication with the unit and arrow intact, so that an oversized rate is not silently misrepresented.

## Implementation Decisions

- Extend the existing core Operator Menu model, toggle snapshot/current setting input, action dispatch and native menu adapter. Reuse children, radio items and token lifecycle; no new menu framework. Re-selecting the active position is a no-op.
- Add persisted Speed Text Position to Settings and load it before initial Rate Strip embedding. Use `HKCU\Software\Win Mon`, `SpeedTextPosition`, REG_DWORD: 0=Right, 1=Left. Validate type, size and value strictly; missing/invalid/unreadable means Right. Existing operator values and Network to Monitor semantics remain intact.
- Extend RateStrip placement and rendering together, not just the text alignment flag. All Embed, Refresh, Relayout, font update and recovery callers must use the selected side. A left strip must not unnecessarily depend on discovery of the notification area when that geometry is irrelevant to its verified free region.
- Right retains the existing notification-area anchor and 4 DIP placement margin. Left starts 4 DIP from the taskbar client area's left edge, vertically centered. This is a horizontal location choice on a bottom taskbar, not support for a vertically positioned Windows taskbar.
- Both rows share numeric, unit and arrow column sizes. The numeric column is right-aligned and the unit column left-aligned. Right unit and arrow columns have fixed positions relative to the strip's right edge; Left arrow and unit columns have fixed positions relative to its left edge. Rate changes alter only the numeric glyphs' left extent. A wider or narrower unit does not move its left anchor or the arrow.
- Measure with the actual active font, face, style, size and taskbar DPI. Reserve four integer digit slots plus one fractional slot, the decimal point and necessary drawing allowance using the widest measured digit, not an assumed width for `9`. Unit width is the maximum measured K/s, M/s and G/s width; arrow width is the maximum ↑/↓ width. Use the measured font space for each column gap, with a 1 DIP minimum. Verify the complete draw extents, including overhangs, fit these reservations.
- Retain current base-1000 formatting, one decimal place and existing boundary rounding behavior. The normal capacity includes `1000.0`; do not change shared FormatRate or the Floating Rate Display merely to enforce a three-digit maximum.
- If an actual numeric string exceeds the reserved column, draw `…` within that column instead of a partial number. Unit and arrow remain complete at their anchors; do not widen the strip on individual samples, cap the measured rate or add new units. If available geometry cannot fit the complete fixed layout, hide the strip and retry rather than crop units/arrows.
- Font/DPI changes, shell recreation and primary monitor changes rebuild measurements and placement. Normal samples refresh position/theme and rates without measuring a new window width from current values. Retain existing startup fallback font, theme behavior and font-setting failure recovery.
- Reuse OperatorSettingChange's live-apply/persist/rollback sequence and existing user error feedback. Persistence or live-apply failure restores the previous selection. If rollback itself fails, preserve the previous saved target, invoke existing recovery behavior and do not report success or show a misleading selected state. A missing taskbar or occupied left region is a temporary display condition: saving the target selection succeeds and recovery later applies it.
- Left region availability must be verified against relevant shell control geometry, including Widgets, Start/search and app icons. Do not resize, move, reserve or otherwise alter shell controls. If availability cannot be established, hide this Rate Strip and retry on existing refresh/recovery paths; keep the saved side and other surfaces active. A safe occupation check is required before treating Left as delivered.
- Preserve click-through when Right-Click Speed Text is off; enabled right-click still opens only Operator Menu and does not activate the strip. Keep support restricted to Windows 11's primary bottom taskbar.
- Implementation scope reaches RateStrip lifecycle/drawing, the core menu state/actions, native host dispatch/startup/recovery, Settings validation, related menu and transaction assertions, registry smoke cleanup, CMake test registration if a native test target is needed, user-facing README and domain documentation. No resources or new runtime dependencies are required by the proposed design.
- This extends ADR-0001's notification-side-only scope through its proposed addendum and follows ADR-0007's persistence strategy. Implementation must reconcile the existing Rate Strip and Operator Menu definitions in CONTEXT with the delivered behavior; current historical descriptions remain explicitly distinct from the proposed term during this document-only phase.

## Testing Decisions

- Test visible behavior and persisted results, not private helper organization. Reuse existing core menu and OperatorSettingChange tests for default Right, submenu structure/order, exclusive radio checks, side actions, no-op re-selection, failed live apply, failed persistence and failed rollback recovery. Existing assertions that assume exact menu counts/order must be updated.
- Prefer one native RateStrip input-to-layout/render seam for geometry. Feed controlled readings into the public surface; observe column positions and rendered bounds. If needed, expose only the minimal native layout result for deterministic verification, not a separate renderer framework or a private-function test for every measurement step.
- Cover `0.0`, `9.9`, `99.9`, `999.9`, `1000.0`, alternating short/long values and K/M/G changes on both rows and both sides. Assert that arrow positions, unit left anchors, numeric right anchors and overall size remain constant across rate samples. Verify explicit numeric overflow leaves the unit and arrow intact.
- Cover proportional fonts with different digit widths and different unit widths, bold/italic, minimum/maximum supported sizes, and 96/144/192 DPI. Font/DPI changes must recompute adequate extents; sampling must not resize them. Preserve existing shared formatting assertions, including `1000.0 M/s`.
- Use Windows registry smoke testing for absent, valid Left/Right, wrong type, unknown DWORD and restart retention. Snapshot and restore the new value in smoke cleanup alongside existing settings; never overwrite the operator's real preference permanently.
- Real Windows 11 desktop acceptance must cover Widgets on/off, centered/left-aligned taskbar icons, Start/search opening, crowded app icons, notification-area width changes, light/dark themes, taskbar unavailability and return, Explorer restart, font switch and primary-monitor/DPI changes. Verify collision handling, unchanged shell control geometry, both click modes and menu accessibility while the strip is hidden.
- Acceptance for Left requires a demonstrated left-end free-region case where the strip is visible and usable, plus occupied/unknown-region cases where it hides safely and restores when free. A permanently hidden implementation does not satisfy this feature.
- Use CMake only: configure x64, build Release, then CTest Release with output on failure from a Visual Studio developer PowerShell. No builds or tests are run by this specification workflow.

## Out of Scope

Secondary taskbars, vertical/top taskbars, Windows 10, automatic side switching, moving/resizing shell controls, user-adjustable column widths or gaps, new units or changed rounding, floating-display redesign, new rendering dependencies, implementation tickets, prototypes, code changes, publication and commits.

## Further Notes

- [Completed grill and rationale](grill.md); [domain vocabulary](../../CONTEXT.md); [taskbar embedding decision](../../docs/adr/0001-taskbar-rate-strip-embed.md); [primary bottom taskbar restriction](../../docs/adr/0002-win11-primary-bottom-only.md); [settings persistence](../../docs/adr/0007-registry-persisted-operator-settings.md).
- Automatically selected assumptions: exact English menu label and order; left-edge 4 DIP anchor; key name/encoding; four-digit numeric capacity; font-space column gaps; numeric overflow ellipsis; hide-and-retry collision behavior; testing seam. These are recommended decisions synthesized under the invoked skill, not separately confirmed user statements.
- Evidence unknown: stable detection of free left-end geometry across Windows 11 builds. Conservative plan: unknown geometry means hidden; implementation must inspect the actual supported shell surfaces and prove both usable and occupied cases before completion. If reliable detection cannot be achieved within existing embedding constraints, record the blocker and revisit the placement decision rather than silently claim support.
- Existing code proves neither universal free space at the left end nor a strict numeric maximum of 999.9. These limitations are explicitly reflected in the capacity and verification requirements.
