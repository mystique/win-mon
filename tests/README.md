# 测试指南

测试使用 Catch2 3.16.1。CMake 自动下载该版本源码并校验 SHA-256。每个 `TEST_CASE` 独立注册到 CTest。测试失败时，输出用例名、断言和源码位置。Catch2 不进入产品发行包。

## 术语

- **隔离 HKCU**：重定向到临时注册表分支的 `HKEY_CURRENT_USER`。
- **桌面验收**：在真实交互式 Windows 11 桌面上运行的端到端测试。
- **视觉取证**：生成图片供人工检查的辅助工具 `visual-evidence`。

## 分界线

### `unit/`

- 验证范围：速率计算、网络选择和菜单模型；设置事务、恢复策略、动画状态和读数解析。
- 可以依赖：内存快照、注入的数据源和显式时间。
- 不得依赖或执行：访问真实注册表、Explorer 或鼠标；等待真实动画；绘制图片。

### `integration/`

- 验证范围：注册表与权限、登录启动项、离屏绘制和原生悬浮窗。
- 可以依赖：隔离 HKCU、Windows API 和测试自建窗口。
- 不得依赖或执行：修改用户设置、系统动画偏好或真实任务栏；移动鼠标；依赖 Explorer。

### `acceptance/`

- 验证范围：完整进程、单实例、正常退出和 Shell 恢复；原生菜单、宿主设置回滚和任务栏输入。
- 可以依赖：交互式 Windows 11 桌面和实际程序。
- 不得依赖或执行：把环境不满足当成通过；强杀进程代替正常退出验证。

### `visual/`

- 验证范围：生成不同 DPI、可见状态、背景和动画帧的图片。
- 可以依赖：实际渲染器和指定输出目录。
- 不得依赖或执行：把文件写出成功当成视觉正确性测试。

### `support/`

- 验证范围：共享网卡数据、注册表隔离、窗口和 GDI 资源生命周期。
- 可以依赖：各层最少的支撑设施。
- 不得依赖或执行：实现另一个测试框架或复制产品算法。

当前产品只支持 Windows 和 MSVC。这里的「单元」指不依赖外部状态，不指跨平台。网络读取器的输入结构、动画和 LED 类型仍来自 Windows SDK。

## 默认验证

环境必须具备以下工具：

- MSVC
- MFC
- Windows SDK
- CMake

在项目根目录执行以下命令：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

默认配置只注册单元与集成测试。集成测试可以创建短暂的悬浮窗口。集成测试不得操作真实任务栏、移动鼠标或修改全局动画设置。

以下命令用于按标签或名称筛选测试：

```powershell
ctest --test-dir build -C Release -L unit --output-on-failure
ctest --test-dir build -C Release -L integration --output-on-failure
ctest --test-dir build -C Release -R "Selecting a NIC" --output-on-failure
ctest --test-dir build -C Release -N
```

测试 Debug 构建时，把 `-C Release` 改为 `-C Debug`。CI 同时构建并验证 Debug 和 Release。使用 `ctest --output-junit <文件>` 可以导出机器可读结果。

首次配置必须连通 Catch2 源码下载地址。后续构建复用 `build/_deps`。离线环境可以传入 `-DFETCHCONTENT_SOURCE_DIR_CATCH2=<已解压的同版本源码目录>`。传入 `-DBUILD_TESTING=OFF` 时，CMake 不下载测试依赖。

## 桌面验收

在普通权限的交互式 Windows 11 桌面上执行：

1. 退出正在运行的 Win Mon。
2. 启用桌面验收并构建测试：

```powershell
cmake -S . -B build -A x64 -DWINMON_INTERACTIVE_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release -L acceptance --output-on-failure
```

这一层必须使用 PowerShell 7。任务栏用例会移动鼠标，也会短暂创建任务栏子窗口。C++ 宿主用例使用隔离 HKCU。进程验收脚本备份并恢复所有会修改的设置，只清理自己启动的进程。只有异常清理时才可以强制结束进程。

任务栏左侧区域不可用时，对应独立用例显示 **Skipped**。其余验收继续执行。显示 **Skipped** 不代表左侧输入和占位行为已通过验证。验证左侧功能时，必须在左侧空闲的桌面上运行并确认该用例实际执行。

集成测试与验收测试共用 CTest 的 `winmon_desktop` 资源锁，防止并行用例互相干扰。注册表重定向作用于整个进程。因此，测试不得在同一进程的工作线程中并发执行注册表用例。

完成验收后，执行以下命令恢复默认配置：

```powershell
cmake -S . -B build -A x64 -DWINMON_INTERACTIVE_TESTS=OFF
```

## 视觉取证

执行以下命令生成视觉取证图片：

```powershell
cmake --build build --config Release --target visual-evidence
```

图片输出在 `build/visual-evidence/`。此工具不在默认构建或 CTest 中运行，也不比对基准图片。自动渲染测试另行验证以下项目：

- 裁剪
- 字形
- 透明度
- DPI
- 颜色
- 状态变化期间的不变区域

## 增加用例

编写新用例时必须遵守以下规则：

1. 按依赖选择层级，再按行为选择文件。不得把模型、持久化、窗口和截图合成一个场景。
2. 给 `TEST_CASE` 使用行为名称。用 `SECTION` 划分共享初始状态的不同分支。用数据生成器覆盖同一规则的输入。
3. 每个用例自己建立状态。资源和进程在失败时必须清理。不得依赖前一用例的采样、注册表值或执行顺序。
4. 用 `REQUIRE` 检查后续步骤的前提。用 `CHECK` 报告可以继续验证的结果。循环输入用 `CAPTURE` 标明。
5. 时间逻辑直接推进显式时钟。等待窗口消息时，等待具体结果并设置超时，不得使用固定睡眠推测完成。
6. 验证设置失败时，必须同时检查运行状态和持久化状态。测试入口保护时，必须使用实际可用性。
7. 新文件必须显式加入本目录的 `CMakeLists.txt`，不得依赖文件通配符自动发现。

关键故障路径分布在以下文件：

- 速率与多网卡选择：`unit/NetworkSessionTests.cpp`
- 恢复重试：`unit/ShellLifecycleTests.cpp`
- 事务分支及调用顺序：`unit/SettingTransactionTests.cpp`
- 启动修复与拒绝写入：`integration/SettingsTests.cpp`
- 实际宿主设置回滚与临时托盘兜底：`acceptance/OperatorActionTests.cpp`
- 单实例、正常退出和 Shell 消息响应：`acceptance/windows-smoke.ps1`
