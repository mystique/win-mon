# 03: 跨屏缩放与运行恢复

Status: ready-for-human
Implementation: complete
Acceptance: pending manual Windows scenarios
Blocked by: None — 01 implementation complete; manual acceptance remains tracked separately

**What to build:** Floating Rate Display 在不同缩放显示器、显示器移除和绘图资源失效后仍保持清晰、可访问和可操作，并在持续运行及隐藏时维持轻量资源使用。

## Parent

Compact Floating Rate Display — Direct2D / DirectWrite 实现规格。基于工单 01 的真实窗口及可离屏绘图入口完成，动态弧段和曲线不是本工单的前置条件。

## Implementation constraints

- 延续 ADR-0003 的薄 MFC 宿主、ADR-0007 的现有设置持久化、ADR-0008 的独立 Floating Rate Display 行为。
- 完善浮动窗口自己的 DPI 处理，不只监听隐藏消息宿主。重建依赖目标的资源时保留窗口和采样状态，不重建整套产品框架。
- 不加入新字体设置、注册表字段、用户缩放控制、主题切换器或备用渲染后端。

## Acceptance criteria

- [x] 创建窗口、恢复到指定显示器、跨屏拖动和 DPI 改变后，均按浮动窗口所在显示器的 DPI 重算位图、字体、线宽和阴影，不拉伸旧位图，不沿用系统 DPI 导致错尺寸。
- [x] 96／120／144／192 DPI 下主体始终对应 144×48 DIP，阴影另计；取整规则一致，没有裁掉圆环／文字或出现旧帧边缘。
- [x] 用默认字体和较宽的 Rate Font 验收 0.0 K/s、999.9 K/s、999.9 G/s 及更长值，完整呈现数字和单位，排版不跳动。字号适配不改写已保存字体，Rate Strip 继续使用原字号行为。
- [x] 保存位置可用时原位恢复；结合工单 01 的小窗口阈值修正，在 100% 缩放、不同 DPI 和部分可见情况下验证位置判断，不能因主体小于旧阈值而无故归位。
- [x] 显示器被移除或已保存位置无可用主体区域时，返回主工作区右上角；跨屏拖动与重新定位后的持久化行为正确，主体不遗失在屏幕外。
- [x] 拖动不抢焦点，胶囊主体接收拖动和右键 Operator Menu，阴影区域不拦截鼠标；始终置顶、无任务栏／Alt-Tab 项、隐藏／显示正常。
- [x] 图形资源丢失后释放依赖目标的资源并重建；单次渲染或提交失败保留上一成功帧，后续 Rate Sample 重试，不提交半成品或清空桌面显示。初始化失败不保存成功状态，不终止仍可用的 Tray Icon 和 Rate Strip。
- [x] 字体、尺寸及 DPI 未变化时复用绘图资源，不每秒重建整套工厂与文本格式；隐藏时不提交帧，不引入后台绘图循环。退出及反复显示／隐藏后资源正常清理，无持续增长。
- [ ] Explorer 重启后 Tray Icon 和 Rate Strip 按既有 Shell Recovery 规则恢复，Floating Rate Display 仍可操作；任务栏无法承载 Rate Strip 时，浮动显示继续可用。
- [x] 扩展同一个位图测试入口覆盖四种 DPI、字体与长读数组合，检查尺寸、边界及 Alpha 属性，不增加逐函数测试接口或字体像素金图。
- [x] 在现有可控边界验证失败后重试及位置恢复；无法自动触发的 Windows 生命周期情形记录人工验证方式和结果，不为测试建立完整 COM／Windows 模拟层。
- [ ] CMake Release 构建和 CTest 通过；Windows 11 人工验收跨屏、移除显示器、位置重启恢复、字体切换、菜单、Shell Recovery、隐藏／显示和退出。不可执行的设备场景明确列为未验证，不能声称已通过。
- [ ] 提供深浅桌面背景上的真实 100% 尺寸截图和放大边缘图，确认文字可读、透明边缘自然、无明显锯齿；若 02 已完成，同时检查双半环与曲线在各 DPI 下的最终效果。
- [x] 同步领域说明：Floating Rate Display 使用固定深色胶囊和下载优先布局，采用 Rate Font 字体族／样式但自身适配字号；删除“相同两行文字和系统主题颜色”的过时描述，保留 Rate Strip 的独立约定。

## Scope boundary

本工单仅依赖 01，不等待 02。若本工单最后完成，使用两者的最终组合重新运行现有构建、测试及相关窗口验收；工单 02 和 03 的编辑若落在同一实现区域，应协调合入，而非凭空增加功能依赖。

## Comments

用户已确认本工单与 02 都只受 01 阻塞。测试沿用已确认的“确定输入到位图”入口，窗口及真实尺寸观感采用人工验收。

### 2026-09-19 implementation

03 已实现浮动窗口自身 DPI 消息、显示变化时位置恢复、目标资源释放／重建、失败后保留成功位图并重试及资源复用。96/120/144/192 DPI × 两字体 × 四读数的矩阵通过；原生 144 DPI 窗口显示／隐藏、移动不激活、屏外归位、资源计数检查通过。实际混合 DPI 跨屏、物理移除显示器及 Explorer 重启未执行；真实 96 DPI 桌面设备截图尚缺，仅有 96 DPI 离屏原尺寸预览。

- 最终 `cmake -S . -B build -A x64`、Release build 和 CTest：3/3 通过。
- Standards / Spec 双轴审查：初审阴影跨进程穿透问题已用独立 `WS_EX_TRANSPARENT` 分层阴影修复；复审均无剩余发现。
- [验证报告及待人工验收清单](../validation.md)。勾选项表示实现和相应自动检查已完成，不代表未执行的设备场景已人工通过。
- 状态 `ready-for-human`：代码实现完成，保留人工验收项，不将 ticket 虚标为全部验收完成。
