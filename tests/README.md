# 测试指南

测试使用 Catch2 3.16.1，通过 CMake 获取固定版本并校验 SHA-256。每个 `TEST_CASE` 独立注册到 CTest，失败会给出用例名、断言和源码位置。框架不进入产品发行包。

## 分界线

| 目录 | 验证内容 | 可以依赖什么 | 不应做什么 |
| --- | --- | --- | --- |
| `unit/` | 速率、网络选择、菜单模型、设置事务、恢复策略、动画状态、读数解析 | 内存快照、注入的数据源和显式时间 | 访问真实注册表、Explorer、鼠标；等待真实动画；绘制图片 |
| `integration/` | 注册表与权限、登录启动项、离屏绘制、原生悬浮窗 | 隔离 HKCU、Windows API、测试自己创建的窗口 | 修改用户设置、系统动画偏好、真实任务栏或鼠标；依赖 Explorer |
| `acceptance/` | 完整进程、单实例、正常退出、Shell 恢复、原生菜单、宿主设置回滚、任务栏输入 | 交互式 Windows 11 桌面及实际程序 | 把环境不满足当成成功；强杀进程代替正常退出验证 |
| `visual/` | 为人工检查生成不同 DPI、可见状态、背景和动画帧 | 实际渲染器、指定输出目录 | 把“文件写出成功”当成视觉正确性测试 |
| `support/` | 共享的网卡数据、注册表隔离、窗口与 GDI 资源生命周期 | 各层需要的最少设施 | 实现另一个测试框架或复制产品算法 |

当前产品只支持 Windows/MSVC。这里的“单元”表示不依赖外部状态，不表示跨平台：网络读取器的输入结构、动画和 LED 类型仍来自 Windows SDK。

## 默认验证

在有 MSVC、MFC、Windows SDK 和 CMake 的环境中执行：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

默认只注册单元与集成测试。集成测试可以创建短暂的悬浮窗口，但不会操作真实任务栏、移动鼠标或更改全局动画设置。

```powershell
ctest --test-dir build -C Release -L unit --output-on-failure
ctest --test-dir build -C Release -L integration --output-on-failure
ctest --test-dir build -C Release -R "Selecting a NIC" --output-on-failure
ctest --test-dir build -C Release -N
```

可用 `-C Debug` 运行对应 Debug 构建。CI 同时构建并验证 Debug 和 Release。`ctest --output-junit <文件>` 可导出机器可读的结果。

首次配置需要访问 Catch2 源码下载地址；后续构建复用 `build/_deps`。离线环境可以用 `-DFETCHCONTENT_SOURCE_DIR_CATCH2=<已解压的同版本源码目录>`。`-DBUILD_TESTING=OFF` 不下载测试依赖。

## 桌面验收

先退出正在运行的 Win Mon。在普通权限的交互式 Windows 11 桌面中执行：

```powershell
cmake -S . -B build -A x64 -DWINMON_INTERACTIVE_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release -L acceptance --output-on-failure
```

这一层需要 PowerShell 7。任务栏用例可能移动鼠标并短暂创建任务栏子窗口。C++ 宿主用例使用隔离 HKCU；进程验收脚本备份并恢复所有可能被应用修改的设置，只清理自己启动的进程。强制结束进程仅用于异常清理。

左侧区域不可用时，对应独立用例显示 **Skipped**。其余验收继续执行；这不代表左侧输入和占位行为已得到验证。需要验证左侧时，应在空闲左侧区域的桌面上确认该用例实际执行。

默认集成与验收共享 CTest 的 `winmon_desktop` 资源锁，避免并行用例互相干扰。注册表重定向是进程级操作，不要在同一测试进程的工作线程中同时执行注册表用例。

完成验收后可恢复默认配置：

```powershell
cmake -S . -B build -A x64 -DWINMON_INTERACTIVE_TESTS=OFF
```

## 视觉取证

```powershell
cmake --build build --config Release --target visual-evidence
```

输出在 `build/visual-evidence/`。此工具不在默认构建或 CTest 中运行，也不进行图片基准比对。自动渲染测试另行验证裁剪、字形、透明度、DPI、颜色及状态变化期间不应移动的区域。

## 增加用例

1. 按依赖选择层级，再按行为选择文件。不要把模型、持久化、窗口和截图合成一个场景。
2. 给 `TEST_CASE` 使用行为名称。用 `SECTION` 表示共享初始状态的不同分支，用数据生成器覆盖同一规则的输入。
3. 每个用例自己建立状态，资源和进程在失败时也必须清理。不要依赖前一用例的采样、注册表值、窗口或执行顺序。
4. 使用 `REQUIRE` 检查后续步骤的前提，使用 `CHECK` 报告可以继续验证的结果。循环输入用 `CAPTURE` 标明。
5. 时间逻辑直接推进显式时钟；必须等待窗口消息时，等待具体结果并设置超时，不用固定睡眠推测完成。
6. 验证设置失败时，要同时检查运行状态和持久化状态；测试入口保护时，要使用实际可用性。
7. 新文件显式加入本目录 `CMakeLists.txt`，不依赖文件通配符自动发现。

关键故障路径分布：速率与多网卡选择在 `unit/NetworkSessionTests.cpp`；恢复重试在 `unit/ShellLifecycleTests.cpp`；事务分支及调用顺序在 `unit/SettingTransactionTests.cpp`；启动修复与拒绝写入在 `integration/SettingsTests.cpp`；实际宿主设置回滚与临时托盘兜底在 `acceptance/OperatorActionTests.cpp`；单实例、正常退出和 Shell 消息处理在 `acceptance/windows-smoke.ps1`。
