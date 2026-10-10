# 01: 双端半环与按原型比例排列的分区图表

**What to build:** 用户在真实 Floating Rate Display 中看到左右两端相向增长的上传／下载半环，以及对齐半圆、带淡网格和渐变的独立连续曲线。每次采样移动更明显，曲线不越出自己的区域。

**Blocked by:** None (can start immediately).

Status: resolved

- [x] 保留深色胶囊及现有窗口交互，在原生窗口实际呈现新的环与图表；不只交付几何助手。
- [x] 上半上传青绿、下半下载青蓝；各半环从左右端同时向顶部／底部中点增长，两侧长度相等。验证 p=0、0.25、0.5、1、下降及原始速率归零；满值中点无重叠增粗，水平接缝仍清晰。
- [x] 圆环外径 D 与上下图表宽度 W 固定为 5:8；外径包含描边，宽度取裁剪矩形，不含组间距。上下图等宽且分别对齐对应半圆，中央间距以规格为准。
- [x] 实际图表使用低对比网格、连续曲线和向底部渐隐的同色填充；上下网格列对齐，线宽、端帽、网格和填色都严格裁剪，尖峰不能侵入另一图表或未来文字区。
- [x] 可见历史改为 32 次真实 Rate Sample，每秒一次，满窗每次左移图宽/31；不添加中间采样或空白历史。保留图形平滑系数与共同峰值下限，圆弧及 Activity 同用该窗口，网络切换／回退清空历史。
- [x] 为后续文字留出稳定区域，在显示态 132×48 DIP 内保留比例；不得为未来文字独立缩窄图表。
- [x] 沿用离屏和真实窗口测试，验证双端弧、裁剪、真实样本顺序与移动、比例和网络重置；在 96/120/144/192 DPI 输出真实尺寸证据，比例只容许像素取整误差。
- [x] 同步更新现行领域说明中环方向、图表排列和历史标尺约定；保留历史规格。
- [x] CMake x64 Release 构建及 Release CTest 通过，记录该票证据，不直接构建 solution。

## Notes

范围与默认值以已批准的 Floating Rate Display — Punctum LED 与双向半环规格为准；本票交付完整行为及对应测试，保留其他功能。


## Comments

2026-10-11: Implementation and automated verification complete. See [implementation record](../implementation.md) for evidence, review fixes and physical-monitor verification limits.
