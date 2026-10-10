# 01: 稳定 Rate Strip 的数字、单位和箭头布局

**What to build:** 在现有 Right 位置把 Rate Strip 改为固定的数字、单位和箭头栏位。速率长度及单位变化不再带动单位、箭头或整个窗口移动；活动字体与 DPI 决定预留空间，超宽数字明确显示省略号。

**Blocked by:** None (can start immediately).

**Status:** ready-for-human

Implementation: complete
Acceptance: automated and current-desktop checks passed

- [x] Right 位置仍紧邻通知区左侧，保留 4 DIP 定位间距、上传在上和下载在下；顺序为数字、单位、箭头。
- [x] 两行共用栏宽与锚点：数字右对齐、单位左对齐、箭头固定；K/s、M/s、G/s 切换和数字长度变化不改变单位左边界、数字右边界、箭头位置或窗口大小。
- [x] 按实际字体、样式、字号和 DPI 测量最宽数字、三个单位和两个箭头，预留四位整数、一位小数、小数点及必要字形余量；栏间距采用活动字体空格宽度且至少 1 DIP，不假定 9 或 G/s 最宽。
- [x] 保留共享十进制单位和一位小数格式，包括现有 `1000.0 M/s` 输出；不修改 Floating Rate Display 的布局、单位或格式规则。
- [x] 超出数字栏的数值显示完整 `…`，单位和箭头完整保留；不扩窗、不封顶伪造数值、不显示部分数字。任务栏可用区域无法容纳完整布局时暂时隐藏并重试，不裁掉单位或箭头。
- [x] 字体、DPI 或 Rate Strip 重建时重新测量；每秒速率变化不根据当前数值重新伸缩窗口。保留主题颜色、字体应用失败恢复和原有点击行为。
- [x] 从 Rate Strip 输入到实际布局/绘制结果验证 `0.0`、`9.9`、`99.9`、`999.9`、`1000.0`、超宽 G/s 数字、上下行不同单位及连续短长值交替的锚点稳定性。
- [x] 验证比例字体、宽数字/单位、粗体/斜体、支持的字号上下限及 96/144/192 DPI。优先复用已有原生测试边界，必要时只增加最小布局结果观察入口，不先做宽泛重构。
- [x] 使用 CMake 配置 x64、构建 Release 并运行 Release CTest，记录结果；不得直接构建 Visual Studio solution。

## Notes

范围以批准的 Taskbar Speed Text Position 规格为准。此票先交付可独立验证的 Right 稳定布局，Left 切换由 02 交付；复用现有 RateStrip 生命周期和渲染路径，不新增渲染框架或运行时依赖。


## Comments

2026-10-11: Implemented fixed columns and actual-font glyph-bearing reservations. Rendering tests compare complete rows with unclipped references across fonts, units, arrows, both rows, both sides, DPI and overflow; see validation.md in the feature directory. No shared rate-format change.
