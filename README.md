# Win Mon

Win Mon 1.5 在 Windows 11 任务栏上显示实时网速和内存占用。Win Mon 也支持在任务栏上显示 NVIDIA GPU 占用率和核心温度。桌面悬浮窗单独显示网络活动。

Win Mon 没有主窗口。所有设置都在右键菜单中。Win Mon 不需要管理员权限。

## 术语

- **通知区域**：Windows 任务栏右端显示系统图标的区域。
- **托盘图标**：通知区域中的 Win Mon 图标。
- **任务栏读数**：嵌入在主显示器底部任务栏中的监控数值。
- **悬浮窗**：独立于任务栏的桌面置顶监控窗口。
- **右键菜单**：右键单击托盘图标、悬浮窗或任务栏读数后打开的设置菜单。

## 下载与启动

运行环境为 **Windows 11 x64**。

1. 打开 [Releases 下载页](https://github.com/mystique/win-mon/releases)。
2. 下载所选版本的 `WinMon-<version>-windows-x64.zip` 压缩包。
3. 把压缩包全部解压到一个文件夹。
4. 双击该文件夹中的 `WinMon.exe`。

首次启动后，网速和内存读数显示在通知区域旁。通知区域中同时显示托盘图标。右键单击托盘图标，可以打开右键菜单。菜单第一行显示当前版本号。

## 查看网速

- 上行数值表示上传速率。上传箭头为绿色「↑」。
- 下行数值表示下载速率。下载箭头为蓝色「↓」。
- 数值每秒更新一次。单位在「K/s」、「M/s」和「G/s」之间自动切换。

启动时，Win Mon 默认选择「All」。「All」汇总已连接的硬件网络。只想查看单个网络时，在「Monitored Network」中选择对应名称。

虚拟网络和隧道网络不计入「All」。用户可以单独选择虚拟网络或隧道网络。每次重新启动 Win Mon，监控选择都恢复为「All」。

## 设置任务栏读数

右键单击托盘图标，可以调整以下菜单项：

| 菜单项 | 用途 |
| --- | --- |
| 「Show Taskbar Text」 | 显示或隐藏任务栏读数。默认显示。 |
| 「Taskbar Display Content」 | 独立勾选「Network Speed」、「GPU」和「Memory」。默认勾选「Network Speed」和「Memory」。 |
| 「Taskbar Display Position」→「Left」 | 在主任务栏左端显示任务栏读数。 |
| 「Taskbar Display Position」→「Right」 | 在通知区域旁显示任务栏读数。默认使用此位置。 |
| 「Enable Taskbar Context Menu」 | 开启后，用户可以右键单击任务栏读数并打开右键菜单。默认关闭。 |
| 「Show Tray Icon」 | 显示或隐藏托盘图标。默认显示。 |

「Taskbar Display Content」中必须至少保留一项。Win Mon 会保存勾选结果到下次启动。

内存读数保留一位小数。上排显示已用物理内存的百分比。下排显示已用容量，例如「31.2GB」。

GPU 读数上排显示占用率，下排显示核心温度（°C）。启动时，Win Mon 通过 NVIDIA 驱动读取这两项数据。两项都可读时，菜单才列出「GPU」。「GPU」默认不勾选。有多块显卡时，Win Mon 选择首块两项都可读的 NVIDIA GPU。用户只选「GPU」且启动时读取失败，Win Mon 本次改用网速显示。

任务栏读数只支持**主显示器底部的任务栏**。左端空间不足或有其他控件占用时，任务栏读数会暂时隐藏。

## 使用悬浮窗

开启悬浮窗的步骤如下：

1. 右键单击托盘图标。
2. 勾选「Show Floating Display」。

首次开启时，悬浮窗显示在主显示器右上角。悬浮窗保持置顶。用户可以拖动悬浮窗到其他位置或其他显示器。Win Mon 会保存悬浮窗的位置。

| 深色背景 | 浅色背景 |
| --- | --- |
| ![深色背景下的悬浮窗](docs/images/floating-dark.png) | ![浅色背景下的悬浮窗](docs/images/floating-light.png) |

截图中的流量为演示数据。绿色表示上传，蓝色表示下载。圆环和水位反映网络活动。曲线显示近期变化。右侧显示实时数值。

右键单击悬浮窗，可以打开同一个右键菜单：

| 菜单项 | 用途 |
| --- | --- |
| 「Show Floating Display」 | 显示或隐藏悬浮窗。默认隐藏。 |
| 「Show Floating Text」 | 显示或隐藏悬浮窗中的速率数值。 |
| 「Show Floating Charts」 | 显示或隐藏悬浮窗中的曲线图。 |
| 「Floating Display Opacity」 | 调整悬浮窗的不透明度。范围为 10% 到 100%。100% 为完全不透明。 |

开启悬浮窗后，速率数值和曲线图默认显示。同时隐藏这两项时，悬浮窗只保留圆形。

双击悬浮窗的圆形区域，可以逐步显示或隐藏曲线图和速率数值。双击圆形以外的区域不会切换显示状态。

## 自动启动与退出

- 勾选「Start at Sign-in」，可以在登录 Windows 后自动启动 Win Mon。
- 选择「Exit」，可以退出 Win Mon。

显示设置和自动启动设置会保存到下次启动。监控网络的选择不保存。

## 常见问题

### 启动后没有主窗口

这是正常现象。Win Mon 只使用任务栏读数、托盘图标和悬浮窗。请右键单击托盘图标打开右键菜单。

### 任务栏上没有网速

按以下步骤检查：

1. 确认「Show Taskbar Text」已勾选。
2. 在「Taskbar Display Content」中勾选「Network Speed」。
3. 确认主显示器的任务栏位于屏幕底部。
4. 使用「Left」时，尝试切换为「Right」。

任务栏位置不受支持或空间不足时，可以勾选「Show Floating Display」查看网速。

### 网速一直为零

按以下步骤检查：

1. 确认所选网络已连接。
2. 打开网页或下载文件，产生网络流量。
3. 在「Monitored Network」中选择「All」或正在使用的网络。

所选网络断开时，网速显示为零。所选网络从系统中消失时，Win Mon 恢复选择「All」。

### 某个菜单入口无法关闭

Win Mon 必须保留至少一个可用的右键菜单入口。关闭某项会导致没有任何入口时，该项保持勾选并显示为灰色。此时必须先开启另一个入口，再关闭该项。

## 技术文档

模块说明与构建命令见[技术架构](docs/architecture.md)。测试分层与运行命令见[测试指南](tests/README.md)。

## 致谢与许可

任务栏嵌入思路参考了 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor)。

本项目采用 [MIT License](LICENSE)。发行包附带 Punctum 字体的 SIL OFL 许可。
