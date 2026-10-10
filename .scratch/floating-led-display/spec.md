# Floating Rate Display — Punctum LED 与双向半环

Status: ready-for-agent

日期：2026-10-11

## Problem Statement

Win Mon 用户希望保留 Floating Rate Display 的深色胶囊结构和绿／蓝配色，但现有元素未达到已选定效果图的视觉效果。当前圆弧只从右端增长，两条趋势重叠，波浪的活动感不足，文字仍有可配置字体的关联。实际渲染器目前忽略传入的速率文字与字体，因此不仅需要更换字形，还需要真正绘制完整的 Upload Rate / Download Rate。

用户已经选择本次会话最后一张概念图，并补充圆弧增长方式、水波运动、采样移动距离和固定点阵资源要求。本规格发布到本地 issue tracker；本次只编写规格，不实施产品代码。

## Solution

保留紧凑深色胶囊：左侧绿／蓝上下半环与动态水波，右侧为上下独立图表和对应 LED 读数。上传半环与下载半环均从左右端同时向自身中点填充。上、下图表分别对齐圆环的上、下半圆，中间留窄间距；连续曲线和渐变填色严格裁剪在各自区域内。每个文字字符独立使用 5×7、35 盏圆灯，亮灯形成字形，暗灯显示该字符的未点亮位置，字符间留空。

视觉参考保存在本规格旁的 `visual-reference.png`。图片用于元素关系与风格；精确点数、间距、裁剪与动画以以下规则为准，不复制生成图片中的字形错误。

## User Stories

1. As a Win Mon user, I want the existing dark capsule retained, so that the display remains familiar and compact.
2. As a Win Mon user, I want green upload and blue download elements, so that I can distinguish directions immediately.
3. As a Win Mon user, I want upload activity confined to the upper semicircle, so that its meaning stays consistent.
4. As a Win Mon user, I want download activity confined to the lower semicircle, so that its meaning stays consistent.
5. As a Win Mon user, I want each semicircle to fill from both ends toward its midpoint, so that activity looks balanced.
6. As a Win Mon user, I want arcs to retract symmetrically as rates fall, so that changing activity remains easy to read.
7. As a Win Mon user, I want zero rates to clear bright arcs, so that an idle network does not look active.
8. As a Win Mon user, I want clear gaps between the semicircles, so that the two directions remain separate.
9. As a Win Mon user, I want the inner water surface to travel like a wave, so that activity feels alive.
10. As a Win Mon user, I want wave speed to respond to network activity, so that motion communicates changing load.
11. As a Win Mon user, I want visible waves at sustained high activity, so that a full-looking gauge does not appear frozen.
12. As a Win Mon user, I want idle motion to stop, so that the display uses little CPU when there is no traffic.
13. As a Win Mon user, I want Windows animation preferences respected, so that the display remains comfortable.
14. As a Win Mon user, I want upload and download trends in distinct regions, so that the curves do not compete.
15. As a Win Mon user, I want those regions aligned with their corresponding semicircles, so that the layout reads as two coherent rows.
16. As a Win Mon user, I want a narrow gap between chart regions, so that the display remains compact.
17. As a Win Mon user, I want faint chart grids, so that I can judge changes without distracting decoration.
18. As a Win Mon user, I want continuous curves with gradient fills, so that recent trends remain smooth and legible.
19. As a Win Mon user, I want curves and fills contained in their regions, so that peaks never overlap other elements.
20. As a Win Mon user, I want a slightly larger shift per sample, so that recent movement is easier to notice.
21. As a Win Mon user, I want each plotted sample to retain its meaning, so that decorative motion does not imply extra measurements.
22. As a Win Mon user, I want arrows, digits and units in matching round-dot glyphs, so that the display looks consistent.
23. As a Win Mon user, I want each character to show its own 35 lamps, so that the readout resembles individual LED character cells.
24. As a Win Mon user, I want dark lamps limited to character cells, so that the text does not sit on a large dotted panel.
25. As a Win Mon user, I want readable decimals and K/s, M/s and G/s units, so that I can interpret rates accurately.
26. As a Win Mon user, I want stable positions as digit counts change, so that the display does not jump.
27. As a Win Mon user, I want long rates kept complete, so that the display does not silently lose information.
28. As a Win Mon user, I want the LED style to work without installing fonts, so that Win Mon remains easy to run.
29. As a Win Mon user, I want Set Font to continue controlling the Rate Strip, so that my taskbar preference is preserved.
30. As a Win Mon user, I want the Floating Rate Display independent of Set Font, so that its chosen LED appearance stays intact.
31. As a Win Mon user, I want consistent rendering at different monitor DPIs, so that moving the display preserves clarity.
32. As a Win Mon user, I want dragging, the Operator Menu and saved position to keep working, so that this visual change does not disrupt daily use.
33. As a Win Mon user, I want network changes to clear old history and activity, so that I do not confuse two networks.
34. As a Win Mon user, I want contained rendering failures and correct resource cleanup, so that monitoring remains reliable.
35. As a Win Mon user, I want a fixed number of LED character cells in each row, so that the readout behaves like a real dot-matrix instrument.
36. As a Win Mon user, I want unused leading digit cells to remain dim, so that short rates keep the same layout without misleading leading zeros.
37. As a Win Mon user, I want tighter but distinct character spacing, so that the additional fixed cells fit harmoniously in the compact display.
38. As a Win Mon user, I want an Operator Menu option to show or hide floating speed text, so that I can choose whether to see numeric readings.
39. As a Win Mon user, I want that preference remembered across restarts, so that I do not have to configure it again.
40. As a Win Mon user, I want rings and trends to keep working while text is hidden, so that I can still monitor activity visually.
41. As a Win Mon user, I want the capsule to smoothly narrow when speed text is hidden and expand when shown, so that it uses only the space it needs.
42. As a Win Mon user, I want rapid visibility changes to reverse smoothly, so that the display never jumps or flickers.
43. As a Win Mon user, I want the ring diameter and chart width to retain the selected prototype's proportions, so that implementation preserves the visual balance I approved.

## Implementation Decisions

### 范围、结构与现状

- 仅修改 Floating Rate Display 的呈现及其字体关联，沿用现有 C++20、薄 MFC 宿主、Direct2D 离屏渲染、WIC 预乘 Alpha 与分层窗口。显示文字时主体基准为 约 217×48 DIP，隐藏文字时按下述规则缩窄，高度不变；阴影每侧最多 3 DIP。效果图是放大示意，不据此扩大窗口。
- 左侧圆环及水波，右侧先图表后读数。默认上下两行读数均显示，可通过菜单一起隐藏；鼠标悬停不改变文字可见性或读数布局。保留无激活拖动、置顶、右键 Operator Menu、阴影点击穿透、保存位置及显示开关。
- 当前离屏渲染器已有采样历史、Activity、波相位、水位和像素输出；当前 Render 忽略文字和字体参数，没有绘制读数。实施需要接通已有格式化字符串，不将此工作视为只换皮。
- 修改覆盖离屏渲染器、Floating Rate Display 动画宿主、字体设置事务与启动／恢复调用、现有测试与视觉证据、资源许可及相应构建打包。复用现有接口边界，不引入通用主题系统、字体引擎或第二条采样链路。

### 双端向中间增长的半环

- 上半环始终表示 Upload Rate，青绿；下半环始终表示 Download Rate，青蓝。左右水平分界处保留小间隙，暗轨道和亮弧均不能跨越间隙。
- 每个半环独立得到比例 p，限制为 [0,1]。其可用半环角长为 A：从左端和右端各增长 p×A/2；上半两段在顶部相向靠近，下半两段在底部相向靠近。两段总长度为 p×A。
- p=0 不画亮弧或端点亮灯；p=0.5 两侧各填充四分之一可用半环，中部仍暗；p=1 两段在自身半环中点无缝合拢，不留下重复端帽增粗。数值降低时两侧同步后退。
- 不使用概念图中单侧亮弧或孤立端点作为实现要求。圆弧不整体旋转。
- 保留当前图形平滑系数 0.35 和双向共同近期峰值标尺，标尺下限 1000 bytes/s。当前原始速率为零时对应亮弧立即清空；负数、NaN、无穷绘图输入按零处理。近期相对活动并不等于网卡带宽利用率。

### 动态水波

- 使用现有约 30 FPS 原生动画计时器和真实经过时间推进相位，呈现水面横向传播的波，不仅上下整体平移。水位跟随双向近期相对活动，水面渐变和轻微层次遵循参考图。
- 用户原话“根据网络负载增大和减慢动画的速度”在本规格暂解释为负载上升加快、负载下降减慢，属于实现默认解释，不宣称用户已明确指定方向。
- 默认沿用现有周期范围：归一活动 L∈[0,1] 时周期为 2200−1200×L 毫秒；相位连续，采样更新不从头重播。L 沿用 Activity 的含义，不测算物理链路带宽。
- 为避免高活动水面充满后波纹完全消失，默认活动水位目标为圆内有效高度的 15%+65%×L；零活动目标为零。沿用约 450ms 的水位变化尺度平滑过渡。波幅受圆内边界约束，峰值活动仍可看见运动的水线。
- 水波、填色及高光严格裁剪在环内，不能盖过环线。整个 Floating Rate Display 隐藏时停止动画；网络归零回落完成且无宽度过渡时停止计时器。仅隐藏速率文字不停止活跃水波。禁用 Windows 客户区动画时静态显示当前目标水位，不持续推进相位。
- 动画帧只改变水面等视觉状态，不增加 Rate Sample，不推动图表历史或改动速率读数。恢复显示时重新评估计时器，避免隐藏时仍运行或显示后不动。

### 图表、网格与采样移动

- **固定比例：圆环外径 D : 每个折线区域宽度 W = 5 : 8，即 W = 1.6×D。** 选定参考图的圆环外径约 340 像素、图表宽度约 544 像素，比例约 1:1.6；将其规范为精确 5:8，不机械复刻生成图的抗锯齿边界偏差。D 包含暗轨道／亮弧的外侧描边，不是内水面直径或弧线中心线直径；W 是网格及曲线裁剪矩形的左右边界距离，不含与圆环／文字之间的间距。
- 上下两个图表宽度必须相同。实现以 DIP 按同一 D 推导 W，不独立随意调节图宽；各 DPI 下只允许像素栅格化取整误差。显示文字、隐藏文字以及整个收窄／展开过渡均保持此比例，不能靠缩窄图表为固定字符位腾空间。比如 D=32 DIP 时 W=51.2 DIP，此数值仅说明比例，不另行指定圆环直径。
- 左侧圆环外缘的顶部／水平中心线／底部是排版锚点。上图顶与环顶对齐，上图底靠近中心线上方；下图顶靠近中心线下方，下图底与环底对齐。两图等高，中央间距默认 1.5 DIP；读数分别垂直居中于对应半区。
- 青绿上传和青蓝下载曲线独立显示，保持连续曲线，不显示采样点。沿用当前平滑样本与不产生额外过冲的单调插值方式，不为外观另加一次数据平滑。
- 两图共用时间轴和共同峰值标尺。默认可见历史由 48 个缩至 32 个 Rate Sample，最新样本在最右侧；横向步长为图宽/31，相对图宽/47 增加约 52%。仍每秒采样一次，满窗后每次左移一个步长，未填满时只画真实样本，不补造历史。
- 圆弧／Activity 使用相同的 32 次历史窗口，避免另外维护一套不同标尺。这会改变旧峰值退出时间，属于明确采用的默认值。
- 图表底层为低对比网格，上下竖线对齐，默认约 10 列、3 行，透明度起点 8%。不显示坐标标签、粗边框或刻度。实际大小下以不抢过曲线为准。
- 曲线下保留对应颜色渐变，向各自图底渐隐；网格位于渐变与曲线下方。整个网格、线条含线宽和端帽、填色均受对应矩形 clip 约束。尖峰、超量程及边缘值不能越到中央间隔、读数或圆环。
- Network to Monitor 改变或回退时清除双向历史、活动与水位，不混入前一个网络的数据。

### Punctum 字形与独立 35 灯单元

- 可以直接复用 Punctum 的点阵源数据。采用内置、编译时固定的所需字符掩码，按目标 DPI 用 Direct2D 绘制圆灯；不依赖安装 TTF、不运行字体构建工具、不联网取资源。无需把所有可能的完整速率字符串预渲染为图片。
- 上游基线锁定为 Punctum 提交 `56caad6cb7295c98fede26edb196d763e124b75d`。提取上下箭头、0–9、点号、K、M、G、斜杠、小写 s 共 18 个有形字符；空格只产生空白间隔。
- 每个有形字符恰好拥有 5 列×7 行共 35 个灯位，亮／暗灯共享位置、圆径和点距。复用对应源字形前七行，不足七行补暗行。所需小写 s 不存在下伸部，无需第八／九行。
- 每个字符的暗灯只在自身 35 灯格内显示；字符之间与空格处无灯，不绘制整行连续点阵面板、字符边框或底卡。点号也有自己的 35 灯格，只点亮基线中间一灯。
- 用户明确数值容量为 999.9：每行依次为一个方向箭头、三个整数位、一个点号位、一个小数位、三个单位字符，共九个有形字符位，不随当前数值增减。
- 三个整数位右对齐，短读数的前导空位保留各自完整的 35 盏暗灯，不点亮前导零；小数点、小数位、箭头和单位始终固定。前导空位是预留数字单元，与字符间无灯的空白间距不同。上下两行使用相同的单元锚点和尺度。
- 收紧原概念图过宽的字间距，默认相邻字符灯中心之间的跨单元距离为单元内点距的 1.5 倍，保证字符格仍然分开；箭头／数值、数值／单位的组间距起点为主点距的 2 倍。单位沿用较小尺度，整体基线一致。所有间距随 DPI 缩放，以固定最大读数的真实尺寸证据校准，不能随速率动态调整字号、点径或间距。
- 上传箭头／数值青绿，下载箭头／数值青蓝，单位用较低亮度灰蓝。未点亮灯为更暗的灰蓝，亮度起点约主亮灯的 6%–10%，必须明显低于单位亮灯。点径默认约点距的 60%，保留圆点间隙。
- 主箭头与数字一致的点距／圆径，单位可用较小尺度，但仍各自完整 5×7。各字符以共同基线排列；亮灯不是叠加在偏移的另一张格网上。
- 单位和数值遵循现有基数 1000、至少 K/s、固定一位小数及 K/s、M/s、G/s 规则；消费既有核心格式化结果，不另建单位转换。固定数字列右对齐，单位及箭头锚点稳定。
- 必须完整显示 0.0 K/s、999.9 K/s、999.9 M/s、999.9 G/s，均使用相同固定字符位布局。先按最大读数统一选定数值点距与圆径，短读数不放大，长读数不临时缩小，不覆盖图表。验收真实 DPI 下点阵可读性，不能用放大图代替。
- 三位整数容量是浮动文字显示上限，不是核心速率格式化函数的输入上限。若原始格式化值超过当前单位的 999.9，Floating Rate Display 的数值亮灯饱和显示 999.9，单位仍使用核心给出的单位；不截字符串、不显示低位冒充实际值、不改变核心测量与 Rate Strip。除 G/s 大值外，还应覆盖单位转换前舍入得到 1000.0 K/s 或 1000.0 M/s 的边界。饱和显示属于固定字符位的默认限制。
- 对格式化入口的未知字符显式返回渲染失败并保留成功帧，不越界索引字形表或悄悄显示空白。正常产品格式的所有字符必须覆盖。
- 上游字体和字形源数据属于 SIL OFL 1.1，而网站／工具的 MIT 不适用于字形。内置子集保留来源、版权与完整 OFL，派生字形资源保持 OFL；许可证随实际交付产物提供，不能只放开发目录。无需新增许可管理框架。

### 字体设置解耦与文档

- Floating Rate Display 固定采用 LED 字形，不再读取、应用或依赖 Rate Font。移除浮动侧字体设置接口及相关字体对象，更新字体事务、启动／恢复调用和测试，而非保留无意义的空操作接口。
- Rate Strip 的 Set Font、已保存 Rate Font、默认系统字体与失败回滚继续工作。持久化失败仍恢复任务栏字体；浮动渲染故障不再导致任务栏字体设置失败。
- 不删除用户保存的字体，Network to Monitor 仍为会话设置。Operator Menu 中既有字体标签与操作保留；只新增下述浮动文字可见性设置。
- **替代旧约定：**当前 CONTEXT 及旧 Floating Rate Display 规格中的单端圆弧、重叠图表、上传悬停出现和共享 Rate Font 被本规格取代；旧规格保留为历史，实施时更新现行领域说明。
- **与 ADR-0008 的共享 Rate Font 条款冲突：**实施时明确修订该条为任务栏使用 Rate Font、浮动显示使用内置 LED。保留 ADR-0008 的窗口生命周期、DPI、逐像素 Alpha 与动画偏好约定，以及 ADR-0007 的用户设置存储规则。

### 菜单配置：是否显示浮动速率文字

- 在现有 Operator Menu 的 Show Floating Display 后增加勾选项 **Show Floating Speed Text**，沿用当前开关组，不另建设置对话框。该菜单从 Tray Icon、支持右键的 Rate Strip 和 Floating Rate Display 打开时行为一致。
- 默认开启，控制上传／下载两行的箭头、数字、单位及全部暗灯底格一起显示或隐藏。不控制 Rate Strip，不隐藏圆环、水波、网格、曲线或渐变。
- 切换后立即开始平滑过渡，不清空采样历史、不重置水波相位。隐藏文字时箭头、数字、单位和暗灯一起淡出，胶囊收窄至圆环＋原宽度图表＋必要右侧内边距，移除文字所占宽度与组间空白；显示时胶囊展开至原宽度，文字一起淡入，恢复九字符位的固定锚点。
- 圆环和上下图表的位置、尺寸、数据时间轴保持稳定，不通过缩放整张图片制造收窄。高度与圆角半径不变，只移动胶囊右边界；紧凑目标宽度由既有图表右界及右侧内边距计算，不能保留原文字空白区，也不能裁掉图表。
- 默认收起用时 220ms、展开 180ms，宽度和文字透明度共用 ease-out cubic 进度（1−(1−t)^3）。过渡中收到相反切换时，从当前实际宽度和透明度反向推进，剩余用时按剩余距离缩放，不跳回端点或排队播放旧动画。亮灯与暗灯同步透明度，不单独残留底灯。
- 共用现有约 30 FPS 动画计时器；即使网络空闲，宽度过渡未结束也保持计时，结束且无水波活动时停止。禁用 Windows 客户区动画时立即切换到目标宽度和文字可见性。
- 动画每帧同步更新真实主体窗口与独立阴影窗口尺寸、像素缓冲、阴影轮廓和命中区域，不能只改绘图留下可点击的透明文字区。拖动与右键使用当前轮廓；宽度改变不新增速率采样、不重建无关字形资源。
- 默认保持窗口左上锚点稳定；若展开后超出当前显示器工作区，则按现有可见性规则约束目标位置，并与宽度一起平滑调整，整个过渡保持在工作区内。拖动或 DPI／显示器变化时从当前几何重新计算目标，不继续使用失效坐标。持久化仍保存有效桌面位置和文字偏好，不保存中间动画进度。
- 浮动显示关闭时仍可配置此选项；下次打开直接以所存状态的目标尺寸显示，不播放启动展开动画。关闭整个浮动显示不覆盖文字偏好，恢复显示或 Shell Recovery 也不丢失偏好。隐藏整个窗口时取消未完成过渡，重新显示时采用最新目标状态。
- 沿用 ADR-0007 的用户注册表保存方式，新增 FloatingRateTextVisible DWORD，1 为显示、0 为隐藏；缺失、不可读、类型错误或未知值默认显示。实施时将此设置补入 ADR-0007 和当前领域说明。
- 沿用现有设置的实时应用、持久化和回滚事务。保存失败时恢复之前可见性与菜单状态；若实时应用失败则不保存新值，沿用现有失败处理与恢复机制，不使其他监控功能退出。
- 实施覆盖核心 Operator Menu 状态及动作、菜单宿主、设置读写、Floating Rate Display 可见性输入、启动和恢复路径，不只增加一个没有连通配置的菜单项。

## Testing Decisions

- 优先沿用现有离屏 Floating Rate Renderer 测试边界：输入速率、格式化字符串、相位、水位和 DPI，检查输出像素与可观察效果。不为私有缓存、容器长度或绘图调用顺序编写镜像测试。
- 用户对测试范围补充了固定字符位和紧凑字间距要求，纳入同一离屏测试边界；没有提出新增测试框架。
- 半环验证 p=0、0.25、0.5、1 的双端对称、正确半区、满值中央合拢、水平两接缝、归零与回缩；复用现有采样输入与彩色像素断言，替换旧单端方向断言。
- 字形验证 18 字符集合、每字符恰好 35 灯位、亮暗同格、字符间空白、点号基线、单位大小与内容准确。覆盖所有数字与 K/M/G 切换、长读数及未知字符失败保帧。
- 固定布局验证 0.0、1.2、12.8、999.9 的变化：每行恒定九个有形字符位，前导数字格只显示暗灯，箭头、点号、小数位、单位和灯径／点距不移动或缩放。覆盖超量程及单位舍入边界的饱和显示，验证没有低位截断；真实尺寸检查收紧后的间距仍能分辨相邻字符，且读数不侵入图表。
- 复用核心菜单测试验证 Show Floating Speed Text 的位置、勾选状态及动作含义；复用设置测试验证默认值、有效读写、错误类型／未知值回退与事务失败回滚。离屏测试验证关闭后箭头、数字、单位和暗灯均消失，但同相位的圆环及图表像素不变；窗口测试验证立即切换、关闭浮动显示期间配置、重新打开、重启加载与恢复路径。
- 宽度动画验证两个端点及中间帧、尺寸与透明度连续、反向切换无跳变、空闲时过渡可完成、结束后停止计时、系统关闭动画时直接到目标。使用当前局部坐标比对圆环／图表，验证不缩放与不裁切；真实窗口测试验证主体／阴影尺寸同步、缩窄后原文字区域点击穿透、显示器边缘展开不越界、过渡中拖动及 DPI 变化。截图与短片同时覆盖收起和展开。
- 图表以可识别样本序列验证每秒移动、真实样本顺序、旧数据退出、网格对齐与裁剪，尤其峰值线宽不能越界。验证同一采样状态不同水波相位的图表和文字像素保持不变。
- 比例验收：在 96、120、144、192 DPI 和文字显示／隐藏／过渡中间帧中，确认上下图表宽度一致，W/D=1.6；对实际像素边界允许最多 1 个物理像素的取整误差。采用定义明确的圆环外侧轮廓与图表 clip 边界测量，不把内水面或字符间距计入比例。
- 动画验证多个相位有横向运动，持续高活动仍有可见水线；活动升降改变周期，整个窗口隐藏或网络归零且无宽度过渡时停止，禁用系统动画时静止。尽量在现有离屏／窗口测试边界完成；只有计时状态无法可靠观察时才增加最小确定性时间输入，不引入动画框架。
- 沿用真实 Floating Window 测试验证非激活、右键、拖动、阴影、显示恢复、DPI 与资源释放；更新原共享字体断言。沿用 Rate Font 事务测试验证任务栏应用／持久化／回滚，以及字体变化不改变浮动输出。
- 输出 96、120、144、192 DPI 的真实尺寸截图，深浅桌面背景及动画短片，检验暗灯、网格、长读数和渐变；放大参考图不是像素金样，最终按真实尺寸验收。
- 构建、编译与测试全部使用 CMake，Release x64，并通过 CTest；不直接构建 Visual Studio solution。本次规格编写不执行产品构建。

## Out of Scope

- 本次实现代码、资源生成或部署；本次产物只有规格与选定参考图。
- 新的设置面板、LED 字体选择器、动画速度滑杆、网格开关或主题系统。
- 更改 Rate Strip 外观、采样频率、核心单位规则或网卡筛选；浮动文字可见性之外的新持久化设置。
- 带宽容量探测、真实网卡利用率百分比、额外网络统计与仪表。
- 点阵采样点曲线、整行点阵底屏、整环旋转、字符切换翻牌动画。
- 完整字体包安装、通用文字排版、支持所需集合之外的字符、每个速率值一张图片。

## Further Notes

- 视觉参考为会话中选定的独立字符暗灯方案。图片中局部字形与灯数不准确，必须以固定 Punctum 数据及每字符 35 灯规则修正。
- 波速方向、32 次历史窗口、水位范围、灯径／暗灯亮度、网格透明度和中央间距是将描述落为可实施行为的默认值，不是用户逐项指定的数值。实际尺寸微调不得改变双端填弧、独立字符底灯、曲线分区与固定字体的要求。
- 风险主要是 约 217×48 DIP 下多个 5×7 单元的可读性。先用现有离屏工具生成真实尺寸证据并校准，不能直接缩小概念图交付。
- 补充要求以本规格的固定九字符位、999.9 数值容量、菜单文字开关及平滑收窄／展开为准；所存参考图仍为短读数示意，未展示全部预留暗灯位及紧凑态。超量程饱和、过渡时长与缓动是实现默认值；隐藏时保留原宽度的旧方案已被取代，固定布局也不再按当前读数长度缩放。
- 上游资料：[Punctum](https://github.com/sundyme/punctum)、[固定版本字形](https://github.com/sundyme/punctum/blob/56caad6cb7295c98fede26edb196d763e124b75d/src/glyphs.py)、[固定版本 OFL](https://github.com/sundyme/punctum/blob/56caad6cb7295c98fede26edb196d763e124b75d/OFL.txt)、[许可范围](https://github.com/sundyme/punctum/blob/56caad6cb7295c98fede26edb196d763e124b75d/LICENSE)。

## 2026-10-11 implementation correction

User requires retaining the original ring diameter rather than shrinking it to fit 132 DIP. Original outer diameter is 37×12/11 = 40.36 DIP; chart width is 64.58 DIP (5:8). Preserve original ring stroke width, aligned semicircle/chart heights and fixed nine-cell LED geometry; enlarge LED pitch by 40.36/24 relative to the first implementation. Widen the visible body to about 217×48 DIP, overriding the original 132×48 limit. Compact mode removes the enlarged readout width without shrinking charts or ring.
