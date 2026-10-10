# 03: 完善运行时恢复并完成桌面验收

**What to build:** 已选择的 Rate Strip 位置在 Explorer、显示器、字体和任务栏几何变化后继续正确工作；可用空间丢失时安全隐藏，恢复后重新显示。完成真实桌面回归，并将用户说明与最终交付行为对齐。

**Blocked by:** 02 — 增加左右位置菜单与持久化切换。

**Status:** ready-for-human

Implementation: complete
Acceptance: pending manual Windows scenarios

- [ ] 对 Left 和 Right 验证 Explorer 重启、任务栏消失/重建、主显示器切换及 DPI 变化；保持保存的位置选择和网络采样状态，重新测量并按正确侧重建布局。
- [ ] 验证字体切换、粗体/斜体、字号上下限及 96/144/192 DPI，数字变化和单位切换不移动单位/箭头锚点；保留字体失败回滚和无效字体回退行为。
- [ ] 验证 Widgets 开关、居中/左对齐任务栏图标、Start/search 打开以及应用图标拥挤场景。Left 空闲时确实可见；占用或无法确认空间时隐藏，恢复空闲后重现，不自动换侧、不改变系统控件几何。
- [ ] 验证通知区宽度变化、通知区几何暂时不可得和每秒刷新；Right 不遮挡通知区，Left 不依赖无关的通知区状态，整个布局空间不足时隐藏并持续重试。
- [ ] 验证明暗主题、两个方向的固定栏位、正常边界 `1000.0` 和数字溢出显示；单位、箭头完整，位置与窗口宽度不因连续采样抖动。
- [ ] 验证 Right-Click Speed Text 开/关、菜单三个入口及 strip 隐藏时的托盘菜单可用性；保持不激活与点击穿透，Floating Rate Display 继续独立运行。
- [x] 对发现的恢复缺口做最小修复并补充针对性回归，复用现有重试和设置恢复流程，不为验证引入宽泛重构。
- [x] 完善 Windows smoke 覆盖和清理：保存并恢复新增及既有配置，异常退出也执行清理，不留下修改后的用户注册表值或 shell 状态。
- [x] 使用 CMake 完成 x64 Release 构建及 Release CTest，并记录自动检查结果与真实 Windows 11 桌面验证环境、场景和证据；明确未验证的版本/场景，不将一台机器的结果泛化为所有 Windows 11 构建。
- [x] 更新 README 和 CONTEXT 中 Rate Strip、Operator Menu 与 Speed Text Position 描述，消除“只能在右侧”的旧描述；仅在实际交付通过后将相关 ADR 拟议扩展与已实现行为对齐，保留无关历史记录。
- [ ] 最终逐项核对批准的规格：默认 Right、即时切换、配置持久化、固定栏位、字体/DPI 自适应、失败回滚、Shell Recovery、空间不足处理及不干扰系统控件均有验证结果。

## Notes

范围以批准的 Taskbar Speed Text Position 规格为准。02 必须已经交付基本启动/恢复接线、可用 Left 及安全碰撞处理；本票补足动态恢复与完整桌面验收，不允许把 02 应完成的功能全部拖延到此票。若实际 shell 版本无法提供可靠占用几何，记录证据和阻塞，重新讨论定位方案，不静默放宽禁止遮挡的约束。


## Comments

2026-10-11: Runtime wiring, hidden/visible reporting, font refresh, collision retries and smoke cleanup implemented. CTest and current-desktop menu/restart/synthetic Shell Recovery tests pass. Remaining unchecked criteria include combinations that need real system preferences/hardware changes; they are not claimed as passed. In particular the TaskbarCreated test does not replace a real Explorer restart. See validation.md in the feature directory for evidence and the explicit pending matrix.
