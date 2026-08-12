param(
    [string]$ExePath = (Join-Path $PSScriptRoot '..\bin\x64\Release\WinMon.exe'),
    [string]$SnapshotPath = ''
)

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class WindowProbe
{
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    public static List<IntPtr> ChildrenOwnedBy(IntPtr parent, uint processId)
    {
        var result = new List<IntPtr>();
        EnumChildWindows(parent, delegate(IntPtr hwnd, IntPtr ignored) {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == processId) result.Add(hwnd);
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@

$process = Start-Process -FilePath $ExePath -PassThru
try {
    Start-Sleep -Milliseconds 2200
    $taskbar = [WindowProbe]::FindWindow('Shell_TrayWnd', $null)
    if ($taskbar -eq [IntPtr]::Zero) {
        Write-Output 'FAIL: primary Shell_TrayWnd not found'
        exit 1
    }

    $children = [WindowProbe]::ChildrenOwnedBy($taskbar, [uint32]$process.Id)
    $visible = @($children | Where-Object { [WindowProbe]::IsWindowVisible($_) })
    if ($visible.Count -eq 0) {
        Write-Output ("FAIL: Win Mon process {0} owns no visible child under Shell_TrayWnd" -f $process.Id)
        exit 1
    }

    $textPixelsVisible = $false
    $transparentBackgroundVisible = $false
    $opaqueBackgroundVisible = $false
    foreach ($child in $visible) {
        $rect = New-Object WindowProbe+Rect
        [void][WindowProbe]::GetWindowRect($child, [ref]$rect)
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        Write-Output ("child=0x{0:X} rect={1},{2},{3},{4}" -f $child.ToInt64(), $rect.Left, $rect.Top, $rect.Right, $rect.Bottom)
        Write-Output ("visible={0}" -f [WindowProbe]::IsWindowVisible($child))
        $leftReferenceBitmap = New-Object System.Drawing.Bitmap(1, $height)
        $leftReferenceGraphics = [System.Drawing.Graphics]::FromImage($leftReferenceBitmap)
        try {
            $leftReferenceGraphics.CopyFromScreen($rect.Left - 2, $rect.Top, 0, 0, $leftReferenceBitmap.Size)
        }
        finally {
            $leftReferenceGraphics.Dispose()
        }


        $bitmap = New-Object System.Drawing.Bitmap($width, $height)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
            if ($SnapshotPath -ne '') { $bitmap.Save($SnapshotPath) }
            $colors = @{}
            for ($y = 0; $y -lt $height; $y++) {
                for ($x = 0; $x -lt $width; $x++) {
                    $argb = $bitmap.GetPixel($x, $y).ToArgb()
                    if ($colors.ContainsKey($argb)) { $colors[$argb]++ } else { $colors[$argb] = 1 }
                }
            }
            $dominant = ($colors.Values | Measure-Object -Maximum).Maximum
            $nonDominant = ($width * $height) - $dominant
            $dominantEntry = $colors.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1
            $dominantArgb = ([int64]$dominantEntry.Key) -band 4294967295
            $dominantCoverage = $dominant / ($width * $height)
            Write-Output ("screen-colors={0} dominant-argb=0x{1:X8} non-dominant-pixels={2}" -f $colors.Count, $dominantArgb, $nonDominant)
            $matchingBackgroundPixels = 0
            for ($y = 0; $y -lt $height; $y++) {
                $reference = $leftReferenceBitmap.GetPixel(0, $y)
                for ($x = 0; $x -lt $width; $x++) {
                    $actual = $bitmap.GetPixel($x, $y)
                    $delta = [Math]::Abs($actual.R - $reference.R) +
                        [Math]::Abs($actual.G - $reference.G) +
                        [Math]::Abs($actual.B - $reference.B)
                    if ($delta -le 24) { $matchingBackgroundPixels++ }
                }
            }
            $backgroundMatchCoverage = $matchingBackgroundPixels / ($width * $height)
            Write-Output ("background-match-coverage={0:N3} dominant-coverage={1:N3}" -f $backgroundMatchCoverage, $dominantCoverage)
            if ($backgroundMatchCoverage -ge 0.75) { $transparentBackgroundVisible = $true }
            if ($dominantCoverage -ge 0.75 -and $backgroundMatchCoverage -lt 0.75) { $opaqueBackgroundVisible = $true }
            if ($colors.Count -ge 2 -and $nonDominant -ge 20) { $textPixelsVisible = $true }
            $leftReferenceBitmap.Dispose()
        }
        finally {
            $graphics.Dispose()
            $bitmap.Dispose()
        }
    }

    if ($opaqueBackgroundVisible) {
        Write-Output 'FAIL: Rate Strip is still an opaque solid rectangle'
        exit 1
    }

    if (!$transparentBackgroundVisible) {
        Write-Output 'FAIL: Rate Strip background does not match the surrounding taskbar'
        exit 1
    }

    if (!$textPixelsVisible) {
        Write-Output 'FAIL: Rate Strip rectangle is visible, but no rendered text pixels appear on screen'
        exit 1
    }

    Write-Output ("PASS: found {0} visible Win Mon taskbar child window(s) with rendered text" -f $visible.Count)
    exit 0
}
finally {
    if (!$process.HasExited) { Stop-Process -Id $process.Id -Force }
}
