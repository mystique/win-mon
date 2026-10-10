# 02: 增加左右位置菜单与持久化切换

**What to build:** 操作者通过统一的 Operator Menu 即时选择 Rate Strip 的 Left 或 Right 位置，重启后保留选择。Left 使用箭头、数字、单位顺序并保持固定栏位，在可验证的空闲左端区域显示；占用或未知空间暂时隐藏，恢复空闲后重试。

**Blocked by:** 01 — 稳定 Rate Strip 的数字、单位和箭头布局。

**Status:** ready-for-human

Implementation: complete
Acceptance: automated/current-desktop checks passed; remaining menu-entry manual checks and broader shell configurations pending

- [x] 在 Network to Monitor 之后、已有分隔符之前增加英文一级 `Speed Text Position`，悬停显示 `Left` 和 `Right`，恰好一项以单选圆点标记；所有现有菜单入口提供相同选项。
- [x] 默认 Right；选择立即更新位置及绘制顺序。Right 保留通知区左邻定位，Left 距主屏底部任务栏左边缘 4 DIP 并垂直居中；上传在上，下载在下。
- [x] 复用 01 的固定栏位测量。Left 为箭头、数字、单位，Right 为数字、单位、箭头；两侧都保留单位锚点、数字右对齐、字体自适应和明确的超宽数字省略。
- [x] `HKCU\Software\Win Mon` 的 `SpeedTextPosition` 采用 REG_DWORD，0=Right、1=Left。启动首次显示前加载；缺失、类型错误、大小错误、未知值或读取失败回退 Right，不阻止启动。
- [x] 复用现有菜单 children、radio 标志、选择 token 和设置事务。相同项重复选择不写入；即时应用或持久化失败时恢复之前选择并沿用现有错误反馈。回滚失败时保留之前保存的目标、进入恢复流程，不误报成功。
- [x] 任务栏不存在或 Left 区域被占用时允许保存目标选择；显示暂时不可用不作为设置失败，也不静默切换到 Right。任务栏重建及恢复入口均保留所选位置。
- [x] 验证 Left 完整布局范围不覆盖 Widgets、Start/search、应用图标等系统控件；无法证明区域空闲时暂时隐藏并通过现有刷新/恢复路径重试，不移动或缩放系统任务栏子窗口。
- [x] Left 的定位与刷新不因无关的通知区几何暂不可得而失效；主屏底部任务栏支持范围保持不变。
- [x] 至少证明一例 Left 可见、可读且不遮挡控件的空闲区域，以及占用/未知区域隐藏、变为空闲后恢复的场景。若无法可靠识别空闲区域，记录阻塞事实，不将永久隐藏当成已交付 Left。
- [x] 菜单及事务测试覆盖默认项、唯一选中、两侧 action、重复选择、实时应用失败、持久化失败和回滚失败；调整原有精确菜单数量/顺序断言。
- [x] 注册表 smoke 验证有效左右值、缺失、错误类型、未知值和应用重启后的选择；测试前快照并在结束时恢复该值，保留用户原配置。
- [ ] 验证托盘、启用后的任务栏右键和 Floating Rate Display 菜单一致；点击穿透/不激活行为不变，浮动显示自身位置和绘制不受此设置影响。
- [x] 使用 CMake 完成 x64 Release 构建和 CTest，记录结果；更新本票已交付行为的必要说明，不宣称未完成的跨场景验收已通过。

## Notes

范围以批准的 Taskbar Speed Text Position 规格为准。沿用 ADR-0001 的 shell embed 与禁止 task-list surgery 约束、ADR-0002 的 primary bottom taskbar 范围及 ADR-0007 的用户级注册表策略；本票落实 ADR-0001/0007 的拟议扩展。左端空闲区域检测在现有证据中尚未证明，必须在本票中检查实际 shell 几何并验证，不能仅硬编码一个左边坐标。


## Comments

2026-10-11: Implemented the menu, strict persisted value validation, transactional application/recovery and conservative native/XAML full-control occupancy checks. Native keyboard smoke selected Left through the actual menu and verified persistence; application restart fixtures confirmed visible Left on this desktop. Existing generic transaction tests cover failure outcomes, with native position tests covering persistence rollback. Shell layouts not available on this desktop remain in 03.
