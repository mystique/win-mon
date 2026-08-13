# Win Mon

Win Mon 是一款面向 Windows 11 的轻量级任务栏网速监控工具。它在主任务栏通知区域旁显示实时上传和下载速率，不提供主窗口，日常操作均通过托盘图标的右键菜单完成。

## 功能

- 在主任务栏上以两行文字显示上传、下载速率，每秒更新一次
- 支持监控全部活动网络接口或指定网络接口
- 自动使用 `K/s`、`M/s`、`G/s` 单位显示速率
- 支持选择显示字体
- 支持设置登录 Windows 后自动启动
- 可选择是否通过右键单击速率文字打开菜单
- 自动适配 DPI、任务栏主题颜色以及 Explorer 重启
- 以普通用户权限运行，不需要管理员权限

## 系统要求

### 运行

- Windows 11 x64
- 位于主显示器底部的任务栏

速率文字仅显示在主显示器的底部任务栏上；使用副屏任务栏或将主任务栏置于其他方向时，托盘图标仍可使用，但速率文字会隐藏。

### 构建

- Visual Studio 2022，并安装“使用 C++ 的桌面开发”和 MFC 组件
- CMake 3.25 或更高版本
- Windows 11 SDK

## 构建与测试

在已配置 Visual Studio 编译环境的终端中运行：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

构建完成后，程序位于：

```text
build/Release/WinMon.exe
```

## 使用方法

1. 启动 `WinMon.exe`。
2. 通过通知区域中的 Win Mon 图标打开右键菜单。
3. 在 **Network to Monitor** 中选择 **All** 或指定网络接口。
4. 根据需要启用 **Launch at Login**、**Right-Click Speed Text**，或通过 **Set Font...** 调整字体。
5. 选择 **Exit** 退出程序。

除网络接口选择外，登录启动、速率文字右键菜单开关和字体设置会保存到当前用户的注册表中。每次启动时，网络接口选择会恢复为 **All**。

## 实现概览

项目使用 C++20、Win32 和 MFC 构建：

- `WinMonCore`：负责网络速率计算、格式化、网络接口选择和菜单模型，不依赖 MFC 或窗口句柄。
- `WinMon`：作为轻量级 Windows 外壳，负责托盘图标、任务栏速率文字、菜单、定时采样、设置和 Explorer 恢复。
- 速率文字以子窗口方式嵌入主任务栏，并依据通知区域的位置进行布局；程序不会缩放或永久修改任务栏中的其他窗口。

## 已知限制

- 仅支持 Windows 11 x64。
- 仅在主显示器底部任务栏显示速率文字。
- 不支持 Windows 10 任务栏、副屏任务栏或垂直/顶部任务栏布局。
- 与其他向任务栏嵌入内容的软件同时运行时，可能发生显示区域重叠。

## 致谢

感谢 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 项目。Win Mon 在实现 Windows 11 任务栏文字显示时，参考了 TrafficMonitor 将显示窗口嵌入任务栏并在通知区域旁布局的技术思路。

TrafficMonitor 仅作为实现思路的参考；Win Mon 对相关功能进行了独立实现，不链接、包含或依赖 TrafficMonitor 的源码或构建产物。

## License

本项目采用 [MIT License](LICENSE) 授权。
