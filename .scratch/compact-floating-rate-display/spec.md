# Compact Floating Rate Display — Direct2D / DirectWrite

Status: ready-for-human

日期：2026-09-19

## Problem Statement

当前 Win Mon 的 Floating Rate Display 采用较大的圆形外观，装饰刻度占用空间，轮廓和线条存在明显锯齿。用户希望获得适合长期置顶的小型深色显示：快速读出 Upload Rate / Download Rate，同时直观看到两个方向的活动强度和近期下载趋势。

当前实现是 C++20、Win32 和薄 MFC 宿主，使用 GDI 绘图及 UpdateLayeredWindow。硬边区域裁剪和浮动分支统一设置像素 Alpha 的做法不能保留平滑的边缘覆盖率。问题应通过替换浮动显示的绘图路径解决。

## Solution

实现约 3:1 的深色胶囊，主体尺寸为 **132×44 DIP**。左侧圆环内显示 Download Rate 数值和单位；右侧上方显示 Upload Rate，下方显示近期 Download Rate 曲线。

圆环沿水平方向分成独立的上下半环：

- **上半环／上传／青绿色**：从右端出发，经顶部逆时针向左端增长。
- **下半环／下载／蓝色**：从右端出发，经底部顺时针向左端增长。
- 速率降低时对应弧段沿原路径缩短。未填充部分保留暗色轨道；零速率不显示亮弧。

采用 Direct2D 绘制形状、DirectWrite 绘制文字，输出预乘 Alpha 位图，通过现有分层窗口机制呈现。保留拖动、置顶、Operator Menu、显示开关和位置持久化等现有使用方式。

## User Stories

1. As a Win Mon user, I want a compact Floating Rate Display, so that it occupies little desktop space.
2. As a Win Mon user, I want the agreed dark palette, so that the display retains the visual character I selected.
3. As a Win Mon user, I want a smooth capsule silhouette, so that the display does not look jagged.
4. As a Win Mon user, I want thin smooth circular strokes, so that the small indicator remains clear.
5. As a Win Mon user, I want Download Rate emphasized inside the circle, so that I can read receiving activity at a glance.
6. As a Win Mon user, I want Upload Rate visible beside the circle, so that sending activity is also readable.
7. As a Win Mon user, I want a green upper semicircle for Upload Rate, so that direction is recognizable by position and color.
8. As a Win Mon user, I want the upload arc to grow counterclockwise from right to left over the top, so that it follows the agreed behavior.
9. As a Win Mon user, I want a blue lower semicircle for Download Rate, so that it is visually distinct from upload.
10. As a Win Mon user, I want the download arc to grow clockwise from right to left underneath, so that it follows the agreed behavior.
11. As a Win Mon user, I want each arc to respond independently, so that upload and download can show different activity levels.
12. As a Win Mon user, I want inactive arc portions to remain visible as quiet tracks, so that the indicator is understandable at low rates.
13. As a Win Mon user, I want a recent download trend, so that I can notice bursts and drops without opening another window.
14. As a Win Mon user, I want existing rate units and precision, so that the Floating Rate Display agrees with the Rate Strip.
15. As a Win Mon user, I want long readings to remain complete, so that large rates are not misleadingly truncated.
16. As a Win Mon user, I want the display to remain crisp at different Windows scaling settings, so that it works on my monitors.
17. As a Win Mon user, I want to drag the display without stealing focus, so that I can position it while working.
18. As a Win Mon user, I want the right-click Operator Menu, so that existing controls remain accessible.
19. As a Win Mon user, I want my display position and visibility restored, so that I do not rearrange it every launch.
20. As a Win Mon user, I want the display recovered onto an available monitor when necessary, so that it does not become inaccessible.
21. As a Win Mon user, I want the Floating Rate Display available when the Rate Strip cannot be shown, so that monitoring continues.
22. As a Win Mon user, I want zero activity clearly represented, so that idle connections do not appear busy.
23. As a Win Mon user, I want renderer failures contained, so that the Tray Icon and monitoring process remain usable.
24. As a Win Mon user, I want low idle resource use, so that this small utility does not interfere with other applications.

## Implementation Decisions

### 已确认的设计与技术选型

- 保留 C++20、Win32、薄 MFC 宿主及现有 CMake 构建方式；符合 ADR-0003。不迁移应用框架。
- 仅替换 Floating Rate Display 的绘图路径；Rate Strip 的任务栏嵌入、主题、交互和字体尺寸行为保持现有约定。
- Direct2D 负责胶囊、轨道、上传与下载弧段、趋势线及轻微阴影；DirectWrite 负责数字和单位。
- 使用 WIC 的 32 位预乘 BGRA 位图作为离屏目标，经兼容的 32 位 DIB 和 UpdateLayeredWindow 提交，保留逐像素 Alpha。此路径可使用 CPU 渲染，不将它宣传为全程 GPU 加速。
- 仅添加 Windows SDK 系统依赖 Direct2D、DirectWrite、WIC；沿用现有 COM 生命周期策略，先检查宿主初始化，再确定初始化位置。所有资源以 RAII 管理。
- 不再对 Direct2D 输出统一覆盖 Alpha，不进行硬边圆形区域裁剪；边缘及阴影的覆盖率保留到最终提交。完全透明像素的颜色分量为零。
- 透明渲染目标上的文字采用灰度抗锯齿，避免 ClearType 在透明混合时产生彩边。用目标显示器 DPI 直接绘制，不能把一张低分辨率成品位图拉伸。

### 外观与排版

- 主体 132×44 DIP，圆角半径 22 DIP，约 3:1。左侧环外径约 36 DIP，主体内留约 4 DIP 边距，线宽约 1.5 DIP，使用圆头端点。
- 主体深灰蓝以 #222E34 为基准，主要文字近白，Upload Rate 青绿、Download Rate 青蓝；暗轨道低对比但可见。主体保持足够不透明度保证读数，透明度主要用于边缘和轻微阴影。
- 阴影透明边距上限每侧约 3 DIP，另计窗口尺寸；不得把主体尺寸与含阴影窗口尺寸混用。阴影不接收点击，胶囊主体可拖动和打开菜单。
- 左侧为下载数值，下方为向下箭头及单位；右上为向上箭头及完整上传读数，右下为单条下载趋势线。没有 CPU、百分比、加号按钮、标题或图外说明文字。
- 文字尺寸起点：下载数值约 15 DIP、单位约 9 DIP、上传约 12 DIP。最终以真实尺寸可读性为准，而非机械复制放大图。
- 使用现有 Rate Font 的字体族和可支持的样式，布局管理浮动显示的字号以维持紧凑尺寸；不修改已保存的 Rate Font。任务栏仍完整采用用户选定的字号。
- 保留基数 1000、K/s／M/s／G/s、至少 K/s、固定一位小数的产品格式。原型中的整数上传示例不是格式规范；应显示例如 128.0 K/s。
- 从现有格式化数据获得数字和单位，不重复实现速率单位规则。左右区域固定，不随数值跳动；以字体测量适配长读数，必要时仅缩小该数值字号，不能省略数值尾部或单位。至少验收 0.0 K/s、999.9 K/s、999.9 G/s 及更长数值。

### 半环方向及数值映射

- 屏幕坐标 x 向右、y 向下，圆心为 C，半径为 r，比例 p 限制在 [0,1]。上传弧从右端开始，扫过角度 -180°×p；下载弧从右端开始，扫过角度 +180°×p。
- 比例为 0.5 时，上传终点在顶部，下载终点在底部；比例为 1 时，两者终点均在左端。左右分界处可留极小视觉间隙，不能改变半环归属或增长方向。
- 零值直接不绘制亮弧，不能留下圆头形成的亮点。满值也不能画成整圆。
- 以下映射属于本规格选定的实现默认值，用户确认的是形状、颜色和方向，并未指定带宽基准：沿用现有平滑系数 0.35 及近期历史的思路，取最近最多 48 个 Rate Sample 的双向平滑值的共同峰值作为标尺，标尺下限为 1000 bytes/s；各方向的比例为自身平滑值与标尺之比，线性映射并截断至 [0,1]。
- 共同标尺使上下半环可比较；它表示近期相对强度，不表示网卡带宽使用百分比。旧峰值离开窗口时比例允许重新调整，不承诺固定速率永远对应固定弧长。
- 原始当前速率为零时，对应亮弧立即清空；数值始终使用当前 Rate Sample 的格式化结果，平滑只影响图形。非有限或负绘图输入作为零处理。
- 保留每秒一个 Rate Sample 的节奏，不增加常驻高频动画计时器。平滑值变化驱动弧长更新，不实现整环旋转。

### 数据流、窗口及资源

- 复用现有速率传入边界和最多 48 个样本的历史存储；下载曲线按时间从左到右绘制，不足 48 个样本时不能虚构历史。没有样本时显示空轨道；全零曲线显示安静的基线。
- 曲线纵轴采用下载历史峰值、1000 bytes/s 下限，几何限制在绘图区；不得因曲线插值越界或产生负值。不要求新增平滑曲线算法。
- Network to Monitor 切换或发生既有自动回退时重置图形历史和平滑状态，避免把不同 NIC 的历史连成一条曲线；不改变现有核心采样及选择语义。
- 保留 Show Floating Display、拖动、始终置顶、不激活、无任务栏／Alt-Tab 项、右键 Operator Menu、注册表位置及可见性持久化行为；遵循 ADR-0007 和 ADR-0008。
- 窗口创建、跨屏拖动和 DPI 改变均以 Floating Rate Display 所在显示器为准，重新计算位图像素尺寸、文字和阴影。需要完善浮动窗口自身的 DPI 消息处理，不能只依赖隐藏消息宿主的 DPI。
- 修正现有位置有效性判断中固定至少 48 像素可见高度的假设。按当前主体实际像素尺寸夹取可见性阈值，使完全可见的 44 DIP 高主体在 100% 缩放下被判为有效；显示器仍存在且位置可用时不跳回默认位置。
- 字体、尺寸或 DPI 未改变时复用工厂、文本格式和绘图资源，避免每次采样重新创建整套资源。隐藏后不提交绘图帧，不新增后台循环。
- 资源丢失时释放依赖目标的资源并重建；一次渲染／提交失败保留上一成功帧，在后续采样重试。初始化失败继续沿用显示开关的失败处理，不保存错误的成功状态，不终止仍可用的 Tray Icon 和 Rate Strip。
- 在现有窗口宿主旁增加一个可离屏调用的具体浮动绘图单元即可，不引入可插拔渲染接口、多个后端、应用级场景图或新框架。输入包括当前读数、历史、字体与 DPI，输出为可提交的预乘 Alpha 位图。

### 文档兼容性

- 本设计符合 ADR-0003、ADR-0007、ADR-0008 的架构和交互约束。
- 当前领域文档将 Floating Rate Display 描述为“与 Rate Strip 相同的两行文字和系统主题颜色”，这与本次已明确选择的深色胶囊、下载优先布局冲突。实现时同步更新该定义，并明确浮动字号适配规则；不能把本设计静默套用到 Rate Strip。
- 本规格不新增注册表字段、不改变 Network to Monitor 的会话内设置约定。

## Testing Decisions

- 测试关注给定速率后用户能看到的结果，不断言 Direct2D 调用顺序、COM 对象数量或私有函数结构。
- 沿用项目现有 CTest 与无第三方测试框架的 Require 风格。核心已有测试继续运行；不为此次视觉改造重写核心测试。
- 首选一个新的较高层入口：给浮动绘图单元输入确定的读数、历史和 DPI，检查输出位图。通过该入口覆盖数据映射、弧段方向、布局边界和透明度，不为每个小函数另建接口。
- 在无阴影干扰的环位置验收 0、0.5、1 的独立上传／下载弧；单独上传时下半环无上传颜色，单独下载时上半环无下载颜色；零速率无亮弧、不同输入可输出不同长度、历史标尺调整不溢出。
- 在合成黑色和浅色背景前后检查边缘：画布外缘透明，胶囊边缘存在介于 0 和 255 的 Alpha，预乘颜色不大于 Alpha，不出现硬黑边。不要依赖跨系统完全一致的字体像素金图。
- 验收默认字体和较宽字体、长读数、0／不足／满历史、异常绘图输入及 96／120／144／192 DPI；位图尺寸符合主体及阴影的缩放规则，内容不越界。
- 窗口行为做一次 Windows 11 集成人工验收：显示与隐藏、拖动不抢焦点、右键菜单、切换 NIC、保存恢复位置、100% 下小窗口位置有效、跨 DPI 显示器拖动、移除显示器、Explorer 重启、退出后资源清理。复用现有位置判断入口增加小窗口回归用例即可，不为此建立新模拟框架。
- 提供真实 100% 大小截图及放大细节，在深浅桌面背景上核对视觉参考，确认上下半环方向、细线圆头和文字可读性。必须区分实际大小与放大预览。
- 所有配置、编译、测试通过 CMake；Release 构建和 CTest 全部通过，再进行真实 Windows 窗口验收。仅写规格本身不要求编译项目。
- 用户已确认上述测试入口方案：确定的速率序列到位图的集成检查，加上拖动、菜单及真实尺寸观感的人工验收。

## Out of Scope

- 不实现 Qt、WinUI、WPF、WebView2 或 Electron 迁移。
- 不引入 DirectComposition／交换链或为这一个小窗口新增完整 GPU 渲染管线。
- 不更改 Rate Strip 的样式、任务栏支持范围、Tray Icon、Operator Menu 结构或网络采样算法。
- 不增加 CPU／内存／温度、百分比带宽、网络详情页、主题切换器、用户缩放设置或新的设置窗口。
- 不实现常驻 60 FPS 动画、粒子、发光、旋转环、截图作为运行时背景、在线字体或远程素材依赖。
- 不创建实现工单拆分、不开始功能实现、不提交 Git commit；本次交付是实现规格及本地视觉参考。

## Further Notes

- 视觉参考：[双半环深色胶囊预览](visual-reference.png)。这是放大的生成图，用于布局、颜色及方向参考；工程验收以本文的尺寸、格式和行为为准。图中的英文引线说明不属于产品 UI。
- 设计收敛记录：保留第二款下载优先布局，采用第三款约 3:1 比例，恢复项目深色系，尺寸由 144×48 DIP 缩至 132×44 DIP，圆环改为上传在上、下载在下且均从右端开始。
- 用户已指定采用推荐的 Direct2D／DirectWrite 技术选型。本规格对未讨论的比例标尺、字体适配、采样刷新和资源复用给出了可直接实现的默认决定。
- 技术依据：[Microsoft — Layered Windows with Direct2D](https://learn.microsoft.com/en-us/archive/msdn-magazine/2009/december/windows-with-c-layered-windows-with-direct2d)。它描述了 Direct2D、WIC 预乘位图和分层窗口的组合；不是要求复制其中旧示例的全部封装。

## Implementation progress — 2026-09-19

三个 ticket 的实现已完成；Release 构建和 3 项 CTest 通过，双轴复审无剩余发现。真实设备场景尚待人工验收，详见 [验证报告](validation.md) 及各 ticket 状态。原 Out of Scope 中“不开始实现／不提交”的限制属于当时规格编写阶段；本次用户已明确要求执行 implement。
