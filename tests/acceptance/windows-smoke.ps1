param([Parameter(Mandatory)][string]$Executable)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Executable)) { throw "Executable not found: $Executable" }
if (Get-Process WinMon -ErrorAction SilentlyContinue) { throw 'Exit the running Win Mon before desktop acceptance testing.' }

Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class WinMonNative {
    public delegate bool EnumWindowProc(IntPtr window, IntPtr parameter);
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int left, top, right, bottom; }
    [StructLayout(LayoutKind.Sequential)]
    struct Point { public int x,y; }
    [DllImport("user32.dll")] static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] static extern bool SetCursorPos(int x,int y);
    [StructLayout(LayoutKind.Sequential)]
    struct MouseInput { public int x,y; public uint data,flags,time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)]
    struct Input { public uint type; public MouseInput mouse; }
    [DllImport("user32.dll")] static extern uint SendInput(uint count, Input[] inputs, int size);
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    struct NotifyIconData {
        public uint size; public IntPtr window; public uint id, flags, callback;
        public IntPtr icon;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)] public string tip;
        public uint state, stateMask;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=256)] public string info;
        public uint timeout;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=64)] public string infoTitle;
        public uint infoFlags; public Guid guid; public IntPtr balloonIcon;
    }
    [DllImport("shell32.dll", CharSet=CharSet.Unicode)] static extern bool Shell_NotifyIcon(uint message, ref NotifyIconData data);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowProc callback, IntPtr parameter);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, EnumWindowProc callback, IntPtr parameter);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr window, StringBuilder text, int capacity);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder text, int capacity);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string name, string title);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string message);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr SendMessageTimeout(IntPtr window, uint message, IntPtr wParam, IntPtr lParam, uint flags, uint timeout, out UIntPtr result);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool SetProp(IntPtr window, string name, IntPtr value);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr GetProp(IntPtr window, string name);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetMenuString(IntPtr menu, uint item, StringBuilder text, int capacity, uint flags);
    [DllImport("user32.dll")] static extern int GetMenuItemCount(IntPtr menu);
    [DllImport("user32.dll")] static extern bool GetMenuItemRect(IntPtr window, IntPtr menu, uint item, out Rect bounds);
    [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    static WinMonNative() { SetThreadDpiAwarenessContext(new IntPtr(-4)); }

    public static IntPtr FindTopLevel(uint process, string title, string className) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((window, parameter) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner != process) return true;
            var actualTitle = new StringBuilder(256); var actualClass = new StringBuilder(256);
            GetWindowText(window, actualTitle, 256); GetClassName(window, actualClass, 256);
            if ((string.IsNullOrEmpty(title) || title == actualTitle.ToString()) &&
                (string.IsNullOrEmpty(className) || className == actualClass.ToString())) {
                result = window; return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
    public static IntPtr[] Strips(uint process) {
        var result = new List<IntPtr>();
        var taskbar = FindWindow("Shell_TrayWnd", null);
        if (taskbar != IntPtr.Zero) EnumChildWindows(taskbar, (window, parameter) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner == process) result.Add(window);
            return true;
        }, IntPtr.Zero);
        return result.ToArray();
    }
    public static string DescribeWindows(uint process) {
        var result = new StringBuilder();
        EnumWindows((window, parameter) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner == process) {
                var title = new StringBuilder(256); var name = new StringBuilder(256);
                GetWindowText(window,title,256); GetClassName(window,name,256);
                result.AppendLine(name.ToString() + ": " + title.ToString());
            }
            return true;
        }, IntPtr.Zero);
        return result.ToString();
    }
    public static void SendAndWait(IntPtr window, uint message) {
        UIntPtr result;
        if (SendMessageTimeout(window, message, IntPtr.Zero, IntPtr.Zero, 2, 5000, out result) == IntPtr.Zero)
            throw new Exception("Window did not acknowledge message " + message);
    }
    static Point savedCursor;
    static bool cursorSaved;
    public static void RestoreCursor() { if(cursorSaved) SetCursorPos(savedCursor.x,savedCursor.y); cursorSaved=false; }
    public static void ForgetTestTrayIcon(IntPtr sink) {
        var data = new NotifyIconData { size=(uint)Marshal.SizeOf(typeof(NotifyIconData)), window=sink, id=1 };
        if (!Shell_NotifyIcon(2,ref data)) throw new Exception("Cannot simulate Explorer losing the test tray icon");
    }
    public static void ClickMenuLabel(IntPtr popup, string label) {
        UIntPtr result;
        if(SendMessageTimeout(popup,0x01e1,IntPtr.Zero,IntPtr.Zero,2,5000,out result)==IntPtr.Zero)
            throw new Exception("Cannot inspect native menu");
        var menu=new IntPtr(unchecked((long)result.ToUInt64()));
        for(uint i=0;i<GetMenuItemCount(menu);++i) {
            var name=new StringBuilder(256); GetMenuString(menu,i,name,256,0x400);
            if(name.ToString()!=label) continue;
            Rect bounds;
            if(!GetMenuItemRect(IntPtr.Zero,menu,i,out bounds)) throw new Exception("Menu item bounds missing: "+label);
            if(!cursorSaved) cursorSaved=GetCursorPos(out savedCursor);
            if(!cursorSaved || !SetCursorPos((bounds.left+bounds.right)/2,(bounds.top+bounds.bottom)/2))
                throw new Exception("Cannot position pointer on menu item");
            var inputs=new[] {
                new Input { mouse=new MouseInput { flags=2 } },
                new Input { mouse=new MouseInput { flags=4 } }
            };
            if(SendInput(2,inputs,Marshal.SizeOf(typeof(Input)))!=2) throw new Exception("Menu mouse input rejected");
            return;
        }
        throw new Exception("Menu item missing: "+label);
    }
    public static IntPtr FindSubmenu(uint process, IntPtr parent) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((window, parameter) => {
            uint owner; GetWindowThreadProcessId(window,out owner);
            var name = new StringBuilder(256); GetClassName(window,name,256);
            if (window != parent && owner == process && name.ToString() == "#32768") {
                result = window; return false;
            }
            return true;
        },IntPtr.Zero);
        return result;
    }
}
"@

function Wait-Until([scriptblock]$Condition, [string]$Failure) {
    $deadline = [Environment]::TickCount64 + 5000
    do {
        $value = & $Condition
        if ($value -is [IntPtr]) { if ($value -ne [IntPtr]::Zero) { return $value } }
        elseif ($value) { return $value }
        Start-Sleep -Milliseconds 20
    } while ([Environment]::TickCount64 -lt $deadline)
    throw $Failure
}
function Start-WinMon {
    $script:process = Start-Process -FilePath $Executable -PassThru -WindowStyle Hidden
    if (-not $process.WaitForInputIdle(5000)) { throw 'GUI initialization timed out' }
    $script:sink = Wait-Until { [WinMonNative]::FindTopLevel([uint32]$process.Id, 'Win Mon Message Sink', $null) } 'Message sink missing'
    Wait-Until { ([WinMonNative]::Strips([uint32]$process.Id)).Count -eq 1 } 'Expected exactly one taskbar strip' | Out-Null
}
function Open-Menu {
    if (-not [WinMonNative]::PostMessage($sink,0x8002,[IntPtr]::Zero,[IntPtr]::Zero)) { throw 'Cannot request operator menu' }
    return Wait-Until { [WinMonNative]::FindTopLevel([uint32]$process.Id,$null,'#32768') } 'Operator menu missing'
}
function Exit-WinMon {
    $popup = Open-Menu
    [WinMonNative]::ClickMenuLabel($popup,'Exit')
    if (-not $process.WaitForExit(5000) -or $process.ExitCode -ne 0) { throw 'Normal Exit did not complete successfully' }
    if ([WinMonNative]::FindTopLevel([uint32]$process.Id,$null,$null) -ne [IntPtr]::Zero -or
        ([WinMonNative]::Strips([uint32]$process.Id)).Count -ne 0) { throw 'Exit left application windows behind' }
}

# Child processes cannot inherit RegOverridePredefKey. Save every setting the real app can write.
$keyPath = 'Software\Win Mon'
$names = @('TaskbarContextMenuEnabled','TaskbarDisplayPosition','TaskbarTextVisible','TrayIconVisible',
    'FloatingDisplayVisible','FloatingTextVisible','FloatingChartsVisible','FloatingDisplayOpacity','FloatingDisplayPosition')
$before = @{}
$original = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($keyPath)
$keyExisted = $null -ne $original
if ($original) {
    foreach ($name in $names) {
        if ($name -in $original.GetValueNames()) {
            $before[$name] = @{ Kind=$original.GetValueKind($name); Value=$original.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) }
        }
    }
    $original.Dispose()
}
$process = $null
$duplicate = $null
try {
    $key = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($keyPath)
    try {
        foreach ($name in @('TaskbarContextMenuEnabled','TaskbarTextVisible','TrayIconVisible')) { $key.SetValue($name,1,[Microsoft.Win32.RegistryValueKind]::DWord) }
        foreach ($name in @('TaskbarDisplayPosition','FloatingDisplayVisible')) { $key.SetValue($name,0,[Microsoft.Win32.RegistryValueKind]::DWord) }
    } finally { $key.Dispose() }
    Start-WinMon
    $strip = [WinMonNative]::Strips([uint32]$process.Id)[0]
    if (-not [WinMonNative]::IsWindowVisible($strip)) { throw 'Default Right strip is not visible' }
    $duplicate = Start-Process -FilePath $Executable -PassThru -WindowStyle Hidden
    if (-not $duplicate.WaitForExit(5000) -or $duplicate.ExitCode -ne 0) { throw 'Second instance did not exit successfully' }
    if ($process.HasExited -or ([WinMonNative]::Strips([uint32]$process.Id)).Count -ne 1) { throw 'Second launch disturbed the first instance' }

    $probe = 'WinMon.Acceptance.RecoveryProbe'
    if (-not [WinMonNative]::SetProp($strip,$probe,[IntPtr]1)) { throw 'Cannot mark old strip for recovery verification' }
    [WinMonNative]::ForgetTestTrayIcon($sink)
    [WinMonNative]::SendAndWait($sink,[WinMonNative]::RegisterWindowMessage('TaskbarCreated'))
    $recovered = [WinMonNative]::Strips([uint32]$process.Id)
    if ($recovered.Count -ne 1 -or [WinMonNative]::GetProp($recovered[0],$probe) -ne [IntPtr]::Zero -or
        -not [WinMonNative]::IsWindowVisible($recovered[0])) { throw 'Shell recovery did not recreate a usable strip' }

    $popup = Open-Menu
    [WinMonNative]::ClickMenuLabel($popup,'Taskbar Display Position')
    $submenu = Wait-Until { [WinMonNative]::FindSubmenu([uint32]$process.Id,$popup) } 'Position submenu did not open'
    [WinMonNative]::ClickMenuLabel($submenu,'Left')
    Wait-Until { (Get-ItemPropertyValue 'HKCU:\Software\Win Mon' 'TaskbarDisplayPosition') -eq 1 } 'Left choice did not persist' | Out-Null
    Wait-Until { [WinMonNative]::FindTopLevel([uint32]$process.Id,$null,'#32768') -eq [IntPtr]::Zero } 'Menu did not close' | Out-Null
    Exit-WinMon
    Start-WinMon
    if ((Get-ItemPropertyValue 'HKCU:\Software\Win Mon' 'TaskbarDisplayPosition') -ne 1) { throw 'Restart lost the position preference' }
    $strip = [WinMonNative]::Strips([uint32]$process.Id)[0]
    if ([WinMonNative]::IsWindowVisible($strip)) {
        $bounds = [WinMonNative+Rect]::new(); $taskbar = [WinMonNative+Rect]::new()
        $taskbarWindow = [WinMonNative]::FindWindow('Shell_TrayWnd',$null)
        if (-not [WinMonNative]::GetWindowRect($strip,[ref]$bounds) -or -not [WinMonNative]::GetWindowRect($taskbarWindow,[ref]$taskbar)) { throw 'Placement geometry missing' }
        if ($bounds.left -ne $taskbar.left + [Math]::Round(4*[WinMonNative]::GetDpiForWindow($taskbarWindow)/96)) { throw 'Restored Left placement is wrong' }
    } else { Write-Output 'Left region unavailable; visible Left input and occupancy have a separate acceptance case.' }
    Exit-WinMon
    Write-Output 'Application acceptance passed: single instance, shell recovery, native menu, restart, normal Exit.'
}
catch {
    if ($process) {
        Write-Host "Acceptance process $($process.Id), exited=$($process.HasExited)"
        Write-Host ([WinMonNative]::DescribeWindows([uint32]$process.Id))
    }
    if ($duplicate) {
        Write-Host "Second instance $($duplicate.Id), exited=$($duplicate.HasExited)"
        if ($duplicate.HasExited) { Write-Host "Second instance exit code: $($duplicate.ExitCode)" }
        else { Write-Host ([WinMonNative]::DescribeWindows([uint32]$duplicate.Id)) }
    }
    throw
}
finally {
    [WinMonNative]::RestoreCursor()
    foreach ($owned in @($duplicate,$process)) {
        if ($owned -and -not $owned.HasExited) { Stop-Process -Id $owned.Id -Force; $owned.WaitForExit(5000) | Out-Null }
        if ($owned) { $owned.Dispose() }
    }
    $key = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($keyPath)
    try {
        foreach ($name in $names) {
            if ($before.ContainsKey($name)) { $key.SetValue($name,$before[$name].Value,$before[$name].Kind) }
            else { $key.DeleteValue($name,$false) }
        }
        $empty = $key.ValueCount -eq 0 -and $key.SubKeyCount -eq 0
    } finally { $key.Dispose() }
    if (-not $keyExisted -and $empty) { [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey($keyPath,$false) }
}
