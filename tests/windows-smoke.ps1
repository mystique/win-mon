param(
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $root "bin/x64/$Configuration/WinMon.exe"
if (-not (Test-Path $executable)) {
    throw "WinMon.exe not found at $executable"
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
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern uint RegisterWindowMessage(string message);
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
            if ((title == null || actualTitle.ToString() == title) &&
                (className == null || actualClass.ToString() == className)) {
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
$rightClickName = "RightClickSpeedText"
$fontName = "RateFont"
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
function Wait-Until([scriptblock]$condition, [string]$failure, [int]$milliseconds = 5000) {
    $deadline = [Environment]::TickCount64 + $milliseconds
    do {
        $result = & $condition
        if ($result) { return $result }
        Start-Sleep -Milliseconds 50
    } while ([Environment]::TickCount64 -lt $deadline)
    throw $failure
}

$rightClickBefore = Save-RegistryValue $rightClickName
$fontBefore = Save-RegistryValue $fontName
$process = $null
try {
    New-Item -Path $productKey -Force | Out-Null
    New-ItemProperty -Path $productKey -Name $rightClickName -Value 1 -PropertyType DWord -Force | Out-Null
    New-ItemProperty -Path $productKey -Name $fontName -Value ([byte[]](0xFF)) -PropertyType Binary -Force | Out-Null

    $process = Start-Process -FilePath $executable -PassThru
    $sink = Wait-Until {
        [WinMonNative]::FindTopLevel([uint32]$process.Id, "Win Mon Message Sink", $null)
    } "Win Mon message sink was not created"
    $strips = Wait-Until {
        $children = [WinMonNative]::FindTaskbarChildren([uint32]$process.Id)
        if ($children.Count -eq 1) { return ,$children }
        return $null
    } "A supported primary bottom taskbar did not produce exactly one Rate Strip"

    [WinMonNative]::PostMessage($strips[0], 0x0205, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    $popup = Wait-Until {
        [WinMonNative]::FindTopLevel([uint32]$process.Id, $null, "#32768")
    } "Persisted Right-Click Speed Text did not open the Operator Menu"
    [WinMonNative]::PostMessage($sink, 0x001F, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Stop-Process -Id $process.Id -Force
    $process.WaitForExit(5000) | Out-Null

    $process = Start-Process -FilePath $executable -PassThru
    $sink = Wait-Until {
        [WinMonNative]::FindTopLevel([uint32]$process.Id, "Win Mon Message Sink", $null)
    } "Win Mon did not restart with persisted settings"
    Wait-Until {
        $children = [WinMonNative]::FindTaskbarChildren([uint32]$process.Id)
        if ($children.Count -eq 1) { return ,$children }
        return $null
    } "Corrupt Rate Font did not fall back to a usable Rate Strip" | Out-Null

    $taskbarCreated = [WinMonNative]::RegisterWindowMessage("TaskbarCreated")
    [WinMonNative]::PostMessage($sink, $taskbarCreated, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Wait-Until {
        ([WinMonNative]::FindTaskbarChildren([uint32]$process.Id)).Count -eq 1
    } "Shell Recovery did not converge on one Rate Strip" | Out-Null

    Write-Output "Win Mon Windows smoke passed"
}
finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
    }
    Restore-RegistryValue $rightClickName $rightClickBefore ([Microsoft.Win32.RegistryValueKind]::DWord)
    Restore-RegistryValue $fontName $fontBefore ([Microsoft.Win32.RegistryValueKind]::Binary)
}
