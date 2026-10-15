param(
    [string]$Configuration = "Release",
    [switch]$RequireLeft
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $root "build/$Configuration/WinMon.exe"
if (-not (Test-Path $executable)) {
    throw "WinMon.exe not found at $executable"
}
if (Get-Process WinMon -ErrorAction SilentlyContinue) {
    throw 'Exit the running Win Mon instance before smoke testing; the test will not stop another instance.'
}

Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class WinMonNative {
    public delegate bool EnumWindowProc(IntPtr window, IntPtr parameter);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowProc callback, IntPtr parameter);
    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr parent, EnumWindowProc callback, IntPtr parameter);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr window, StringBuilder text, int capacity);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr window, StringBuilder text, int capacity);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int left, top, right, bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct Point { public int x, y; }
    [StructLayout(LayoutKind.Sequential)]
    public struct MouseInput { public int dx, dy; public uint mouseData, flags, time; public IntPtr extraInfo; }
    [StructLayout(LayoutKind.Sequential)]
    public struct Input { public uint type; public MouseInput mouse; }
    [DllImport("user32.dll")]
    public static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")]
    public static extern IntPtr WindowFromPoint(Point point);
    [DllImport("user32.dll")]
    public static extern uint SendInput(uint count, Input[] inputs, int size);
    private static Point savedCursor;
    private static bool cursorSaved;
    public static void RightClick(IntPtr window) {
        Rect bounds;
        if (!GetWindowRect(window, out bounds)) throw new Exception("Rate Strip bounds unavailable");
        var center = new Point { x = (bounds.left + bounds.right) / 2, y = (bounds.top + bounds.bottom) / 2 };
        if (WindowFromPoint(center) != window) throw new Exception("Rate Strip does not receive real mouse hit tests");
        cursorSaved = GetCursorPos(out savedCursor);
        if (!cursorSaved || !SetCursorPos(center.x, center.y)) throw new Exception("Test pointer placement failed");
        var clicks = new[] {
            new Input { mouse = new MouseInput { flags = 0x0008 } },
            new Input { mouse = new MouseInput { flags = 0x0010 } }
        };
        if (SendInput(2, clicks, Marshal.SizeOf(typeof(Input))) != 2) throw new Exception("Test mouse input failed");
    }
    public static void RestoreCursor() {
        if (cursorSaved) SetCursorPos(savedCursor.x, savedCursor.y);
        cursorSaved = false;
    }
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")]
    public static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern uint RegisterWindowMessage(string message);
    [DllImport("user32.dll")]
    public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr dpiContext);
    static WinMonNative() { SetThreadDpiAwarenessContext(new IntPtr(-4)); }
    public static IntPtr FindTopLevel(uint processId, string title, string className) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((window, parameter) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner != processId) return true;
            var actualTitle = new StringBuilder(256);
            var actualClass = new StringBuilder(256);
            GetWindowText(window, actualTitle, actualTitle.Capacity);
            GetClassName(window, actualClass, actualClass.Capacity);
            if ((string.IsNullOrEmpty(title) || actualTitle.ToString() == title) &&
                (string.IsNullOrEmpty(className) || actualClass.ToString() == className)) {
                result = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static IntPtr[] FindTaskbarChildren(uint processId) {
        var result = new List<IntPtr>();
        IntPtr taskbar = FindWindow("Shell_TrayWnd", null);
        if (taskbar == IntPtr.Zero) return result.ToArray();
        EnumChildWindows(taskbar, (window, parameter) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner == processId) result.Add(window);
            return true;
        }, IntPtr.Zero);
        return result.ToArray();
    }
}
"@




$productKey = "HKCU:\Software\Win Mon"
$rightClickName = "TaskbarContextMenuEnabled"
$positionName = "TaskbarDisplayPosition"
$missing = [guid]::NewGuid().ToString()
function Save-RegistryValue([string]$name) {
    try { return (Get-ItemPropertyValue -Path $productKey -Name $name -ErrorAction Stop) }
    catch { return $missing }
}
function Restore-RegistryValue([string]$name, $value, [Microsoft.Win32.RegistryValueKind]$kind) {
    if ($value -eq $missing) {
        Remove-ItemProperty -Path $productKey -Name $name -ErrorAction SilentlyContinue
    } else {
        New-ItemProperty -Path $productKey -Name $name -Value $value -PropertyType $kind -Force | Out-Null
    }
}
function Registry-Kind([string]$name) {
    try { return (Get-Item -Path $productKey -ErrorAction Stop).GetValueKind($name) }
    catch { return [Microsoft.Win32.RegistryValueKind]::Unknown }
}
function Wait-Until([scriptblock]$condition, [string]$failure, [int]$milliseconds = 5000) {
    $deadline = [Environment]::TickCount64 + $milliseconds
    do {
        $result = & $condition
        if ($result -is [IntPtr]) {
            if ($result -ne [IntPtr]::Zero) { return $result }
        } elseif ($result) { return $result }
        Start-Sleep -Milliseconds 50
    } while ([Environment]::TickCount64 -lt $deadline)
    throw $failure
}

function Open-StripMenu([IntPtr]$strip, [uint32]$processId) {
    try {
        [WinMonNative]::RightClick($strip)
        return Wait-Until {
            [WinMonNative]::FindTopLevel($processId, $null, '#32768')
        } 'Real Rate Strip right-click did not open the Operator Menu'
    } finally { [WinMonNative]::RestoreCursor() }
}

function Assert-Centered([IntPtr]$strip) {
    $actual = [WinMonNative+Rect]::new()
    $taskbarRect = [WinMonNative+Rect]::new()
    $taskbar = [WinMonNative]::FindWindow('Shell_TrayWnd', $null)
    if (-not [WinMonNative]::GetWindowRect($strip, [ref]$actual) -or
        -not [WinMonNative]::GetWindowRect($taskbar, [ref]$taskbarRect)) { throw 'Position bounds unavailable' }
    $expectedTop = $taskbarRect.top + [Math]::Floor((($taskbarRect.bottom - $taskbarRect.top) - ($actual.bottom - $actual.top)) / 2)
    if ($actual.top -ne $expectedTop) { throw 'Rate Strip moved away from the vertical center' }
}

$rightClickBefore = Save-RegistryValue $rightClickName
$positionBefore = Save-RegistryValue $positionName
$rightClickKind = Registry-Kind $rightClickName
$positionKind = Registry-Kind $positionName
$process = $null
try {
    New-Item -Path $productKey -Force | Out-Null
    New-ItemProperty -Path $productKey -Name $rightClickName -Value 1 -PropertyType DWord -Force | Out-Null
    Remove-ItemProperty -Path $productKey -Name $positionName -ErrorAction SilentlyContinue

    $process = Start-Process -FilePath $executable -PassThru -WindowStyle Hidden
    if (-not $process.WaitForInputIdle(5000)) { throw 'Win Mon did not finish GUI initialization' }
    $sink = Wait-Until {
        [WinMonNative]::FindTopLevel([uint32]$process.Id, "Win Mon Message Sink", $null)
    } "Win Mon message sink was not created"
    $strips = Wait-Until {
        $children = [WinMonNative]::FindTaskbarChildren([uint32]$process.Id)
        if ($children.Count -eq 1) { return ,$children }
        return $null
    } "A supported primary bottom taskbar did not produce exactly one Rate Strip"

    $popup = Open-StripMenu $strips[0] ([uint32]$process.Id)
    # Exercise the native menu loop: Network, Show Taskbar Text, Position, expand, select Left.
    foreach ($key in @(0x28, 0x28, 0x28, 0x27, 0x0D)) {
        [WinMonNative]::PostMessage($popup, 0x0100, [IntPtr]$key, [IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 50
    }
    Wait-Until { (Save-RegistryValue $positionName) -eq 1 } 'Native Left selection did not persist' | Out-Null
    Wait-Until { [WinMonNative]::FindTopLevel([uint32]$process.Id, $null, '#32768') -eq [IntPtr]::Zero } 'Position menu did not close' | Out-Null
    if ($RequireLeft) {
        Wait-Until { [WinMonNative]::IsWindowVisible($strips[0]) } 'Unoccupied Left disappeared after menu selection' | Out-Null
    }
    if ([WinMonNative]::IsWindowVisible($strips[0])) {
        Assert-Centered $strips[0]
        $popup = Open-StripMenu $strips[0] ([uint32]$process.Id)
        [WinMonNative]::PostMessage($popup, 0x0100, [IntPtr]0x1B, [IntPtr]::Zero) | Out-Null
    }
    [WinMonNative]::PostMessage($sink, 0x001F, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Stop-Process -Id $process.Id -Force
    $process.WaitForExit(5000) | Out-Null

    $process = Start-Process -FilePath $executable -PassThru -WindowStyle Hidden
    if (-not $process.WaitForInputIdle(5000)) { throw 'Win Mon did not finish GUI initialization' }
    $sink = Wait-Until {
        [WinMonNative]::FindTopLevel([uint32]$process.Id, "Win Mon Message Sink", $null)
    } "Win Mon did not restart with persisted settings"
    $restartedStrips = Wait-Until {
        $children = [WinMonNative]::FindTaskbarChildren([uint32]$process.Id)
        if ($children.Count -eq 1) { return ,$children }
        return $null
    } "Persisted settings did not restore a usable Rate Strip"
    if ($RequireLeft) {
        Wait-Until { [WinMonNative]::IsWindowVisible($restartedStrips[0]) } 'Persisted Left disappeared after restart' | Out-Null
    }
    if ([WinMonNative]::IsWindowVisible($restartedStrips[0])) {
        Assert-Centered $restartedStrips[0]
        $popup = Open-StripMenu $restartedStrips[0] ([uint32]$process.Id)
        [WinMonNative]::PostMessage($popup, 0x0100, [IntPtr]0x1B, [IntPtr]::Zero) | Out-Null
    }

    $taskbarCreated = [WinMonNative]::RegisterWindowMessage("TaskbarCreated")
    [WinMonNative]::PostMessage($sink, $taskbarCreated, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Wait-Until {
        ([WinMonNative]::FindTaskbarChildren([uint32]$process.Id)).Count -eq 1
    } "Shell Recovery did not converge on one Rate Strip" | Out-Null

    Stop-Process -Id $process.Id -Force
    $process.WaitForExit(5000) | Out-Null
    foreach ($savedPosition in @('missing', 'wrong-type', 2, 0, 1, 1)) {
        if ($savedPosition -eq 'missing') {
            Remove-ItemProperty -Path $productKey -Name $positionName -ErrorAction SilentlyContinue
        } elseif ($savedPosition -eq 'wrong-type') {
            New-ItemProperty -Path $productKey -Name $positionName -Value '1' -PropertyType String -Force | Out-Null
        } else {
            New-ItemProperty -Path $productKey -Name $positionName -Value $savedPosition -PropertyType DWord -Force | Out-Null
        }
        $process = Start-Process -FilePath $executable -PassThru -WindowStyle Hidden
        if (-not $process.WaitForInputIdle(5000)) { throw 'Win Mon did not finish GUI initialization' }
        $sink = Wait-Until {
            [WinMonNative]::FindTopLevel([uint32]$process.Id, 'Win Mon Message Sink', $null)
        } 'Position fixture failed to start'
        $strip = Wait-Until {
            $children = [WinMonNative]::FindTaskbarChildren([uint32]$process.Id)
            if ($children.Count -eq 1) { return $children[0] }
            return $null
        } 'Position fixture failed to create the Rate Strip'
        if ($savedPosition -eq 1) {
            # A shell provider may time out at launch; allow the normal sample retry to place Left.
            $placementDeadline = [Environment]::TickCount64 + 2000
            while (-not [WinMonNative]::IsWindowVisible($strip) -and [Environment]::TickCount64 -lt $placementDeadline) {
                Start-Sleep -Milliseconds 50
            }
        }
        if ([WinMonNative]::IsWindowVisible($strip)) {
            $actual = [WinMonNative+Rect]::new()
            $taskbarRect = [WinMonNative+Rect]::new()
            $taskbar = [WinMonNative]::FindWindow('Shell_TrayWnd', $null)
            if (-not [WinMonNative]::GetWindowRect($strip, [ref]$actual) -or
                -not [WinMonNative]::GetWindowRect($taskbar, [ref]$taskbarRect)) { throw 'Position bounds unavailable' }
            $leftMargin = [Math]::Round(4 * [WinMonNative]::GetDpiForWindow($taskbar) / 96)
            if ($savedPosition -eq 1) {
                if ($actual.left -ne $taskbarRect.left + $leftMargin) { throw 'Persisted Left was not applied' }
                Write-Output 'Persisted Left is visible on this desktop'
            } elseif ($actual.left -lt $taskbarRect.left + ($taskbarRect.right - $taskbarRect.left) / 2) {
                throw 'Default/invalid/Right position did not use notification-side placement'
            }
        } else {
            Write-Output "Position $savedPosition is temporarily hidden; usable placement is covered by native window tests"
        }
        [WinMonNative]::PostMessage($sink, $taskbarCreated, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        Wait-Until { ([WinMonNative]::FindTaskbarChildren([uint32]$process.Id)).Count -eq 1 } 'Position did not survive shell recreation' | Out-Null
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit(5000) | Out-Null
    }

    Write-Output "Win Mon Windows smoke passed"
}
finally {
    [WinMonNative]::RestoreCursor()
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
    }
    Restore-RegistryValue $rightClickName $rightClickBefore $rightClickKind
    Restore-RegistryValue $positionName $positionBefore $positionKind
}
