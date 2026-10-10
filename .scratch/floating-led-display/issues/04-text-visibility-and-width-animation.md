# 04: 持久化菜单文字开关与平滑收窄／展开

**What to build:** 用户在 Operator Menu 切换 Show Floating Speed Text，浮动显示同步淡出文字并收窄，或展开并淡入；偏好跨重启保存，快速切换、拖动和 DPI 变化保持顺畅。

**Blocked by:** 02：固定九字符位的 Punctum LED 读数及字体解耦；03：随活动变化的动态水波及计时器生命周期。

Status: resolved

- [x] 在 Show Floating Display 后接通勾选项 Show Floating Speed Text，默认开启；所有既有菜单入口一致。核心菜单状态／动作、宿主、实时应用和设置保存构成完整路径。
- [x] 保存 FloatingRateTextVisible 用户 DWORD，0/1 对应隐藏／显示；缺失、错误类型、不可读或未知值默认显示。实时应用失败不保存，保存失败按既有事务恢复偏好、勾选及可见效果。
- [x] 隐藏两行箭头、文字、单位和暗灯时胶囊收窄，显示时展开；圆环与上下图表保持局部位置、尺寸、5:8 比例及历史，不缩放整图，不保留原文字空白区。
- [x] 收起默认 220ms、展开 180ms，使用规格 ease-out cubic，同步淡化亮暗灯；快速反向从当前宽度／透明度继续，按剩余距离缩放时间，不跳端点、不排队旧过渡。
- [x] 与水波共用计时器；网络空闲仍完成宽度过渡，无活动和过渡后停止；仅隐藏文字不停止活跃水波。系统禁用动画时直接到端态。
- [x] 每帧主体和阴影真实尺寸、像素缓冲、阴影轮廓及命中区域同步，缩窄后原文字区点击穿透。保留不激活、拖动和右键，不留下透明可点击尾部。
- [x] 默认左上锚点稳定，显示器边缘展开时按规格平滑约束目标位置；过渡中拖动、DPI／显示器变化重算几何，不越界、不裁图。持久化有效位置而非动画进度。
- [x] 浮动窗口关闭时可配置，打开时直接以最新目标尺寸显示；重启、恢复及关闭过程中切换不丢失偏好，关闭整个窗口取消过渡。
- [x] 复用菜单、设置事务、离屏和真实窗口测试验证勾选、默认值、回滚、完整文字消失、端态／中间帧、反向、计时器、点击穿透、边缘、DPI 和恢复。输出多 DPI 收起／展开真实大小证据。
- [x] 更新现行领域说明及 ADR-0007 的新增偏好；对四票组合完成一次最终深浅背景、多 DPI、短／满读数、动画与恢复验收，保留许可交付证据。
- [x] CMake x64 Release 配置、构建及 Release CTest 通过，记录最终结果和未覆盖项，不直接构建 solution。

## Notes

范围与默认值以已批准的 Floating Rate Display — Punctum LED 与双向半环规格为准；本票交付完整行为及对应测试，保留其他功能。


## Comments

2026-10-11: Implementation and automated verification complete. See [implementation record](../implementation.md) for evidence, review fixes and physical-monitor verification limits.
