# 03: 随活动变化的动态水波及计时器生命周期

**What to build:** 用户看到环内持续传播的水波，速度和水位响应网络活动，高负载仍保有可见水线；空闲、窗口隐藏和系统动画偏好下行为正确。

**Blocked by:** 01：双端半环与按原型比例排列的分区图表。

Status: resolved

- [x] 真实窗口使用现有约 30 FPS 原生计时器推进横向传播波形；按真实经过时间保持相位连续，采样到达不从头重播。
- [x] 采用规格默认活动映射：周期 2200−1200×L 毫秒，活动水位目标 15%+65%×L，零活动目标零，约 450ms 平滑回落；高活动仍可见动态水面，波峰及填色裁剪在环内。
- [x] 动画只更新视觉，不添加 Rate Sample、不移动图表历史、不修改读数；不同相位下图表和任何已有文字输出保持不变。
- [x] 活跃窗口显示时运行，零活动回落完成后停止，整个窗口隐藏时停止，重新显示时正确恢复；网络切换清空旧活动和水位。
- [x] Windows 禁用客户端动画时使用静态目标水位，不推进相位；计时器或渲染失败沿用现有受控处理。
- [x] 本票不抢先实现文字开关；后续 04 在同一个计时器生命周期中加入宽度过渡运行条件，不新增平行动画计时器。
- [x] 用现有离屏相位输入及真实窗口消息验证传播运动、高活动、升降速、闲置／隐藏恢复、关闭动画和资源释放；输出真实大小动画证据，不只提交放大演示。
- [x] 更新现行领域说明中的活动动画行为；CMake x64 Release 构建及 Release CTest 通过，记录证据。

## Notes

范围与默认值以已批准的 Floating Rate Display — Punctum LED 与双向半环规格为准；本票交付完整行为及对应测试，保留其他功能。


## Comments

2026-10-11: Implementation and automated verification complete. See [implementation record](../implementation.md) for evidence, review fixes and physical-monitor verification limits.
