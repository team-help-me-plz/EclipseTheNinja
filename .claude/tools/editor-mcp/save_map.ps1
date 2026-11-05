param(
    [Parameter(Mandatory=$true)][string]$Ext
)
# WP 현재 레벨 저장 : 툴바 "현재 레벨 저장" 클릭 -> 실패하면 레벨 탭 포커스 후 Ctrl+S
# is_dirty 는 WP 외부 액터 변경을 못 잡으므로 __ExternalActors__\<Ext> 아래 새로 쓰인 파일 수로 확인한다
# 예) save_map.ps1 -Ext "Maps\Week_3\Lvl_Week_3_Prototype_1"
$s = $PSScriptRoot
$work = Join-Path $env:TEMP "EclipseMcp"
New-Item -ItemType Directory -Force $work | Out-Null
$a = Join-Path $work "a_save.json"
$snap = Join-Path $work "snap_save.txt"
$call = Join-Path $s "call.ps1"
$root = (Resolve-Path (Join-Path $s "..\..\..")).Path

function Get-CurLevel {
    $r = & $call -Toolset editor_toolset.toolsets.scene.SceneTools -Tool get_current_level
    return ([regex]::Match(($r -join ''), '"returnValue":"([^"]*)"')).Groups[1].Value
}
function Click-Ref([string]$ref) {
    [System.IO.File]::WriteAllText($a, '{"ref":"' + $ref + '","button":"left","doubleClick":false,"modifiers":{"bShift":false,"bCtrl":false,"bAlt":false,"bCmd":false}}')
    & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Click -ArgsFile $a | Out-Null
}

# 최소화돼 있으면 Slate가 창을 못 보고, 포커스를 안 주면 클릭이 성공해도 저장되지 않는다 -> 복원 + 전면으로
Add-Type @"
using System; using System.Runtime.InteropServices;
public class WinU { [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h); [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h); }
"@
$ed = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { (Get-CimInstance Win32_Process -Filter "ProcessId=$($_.Id)").CommandLine -match 'EclipseTheNinja.uproject' } | Select-Object -First 1
if (-not $ed) { "editor not running"; exit 1 }
$wasIconic = [WinU]::IsIconic($ed.MainWindowHandle)
function Restore-Min { if ($wasIconic) { [WinU]::ShowWindow($ed.MainWindowHandle, 7) | Out-Null } }

$lvl = Get-CurLevel
[WinU]::ShowWindow($ed.MainWindowHandle, 9) | Out-Null
[WinU]::SetForegroundWindow($ed.MainWindowHandle) | Out-Null
Start-Sleep -Seconds 1
$extDir = Join-Path $root ("Content\__ExternalActors__\" + $Ext)
$startT = Get-Date
function Get-NewCount { return @(Get-ChildItem $extDir -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $startT }).Count }
for ($try = 0; $try -lt 3; $try++) {
    if ($try -gt 0 -and (Get-NewCount) -gt 0) { Restore-Min; "saved files: " + (Get-NewCount); exit 0 }

    # 메인 창을 다시 관찰해 최신 참조를 얻는다
    [System.IO.File]::WriteAllText($a, '{"ref":"","maxDepth":0}')
    & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Observe -ArgsFile $a | Out-Null
    [System.IO.File]::WriteAllText($a, '{"ref":"","maxDepth":1,"bIncludeSourceLocations":false}')
    & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Snapshot -ArgsFile $a -Out $snap | Out-Null
    $w = [regex]::Match([System.IO.File]::ReadAllText($snap), 'window \\"EclipseTheNinja[^\[]*\[[^\]]*\] \[ref=(\w+)\]')
    $win = $w.Groups[1].Value
    [System.IO.File]::WriteAllText($a, '{"ref":"' + $win + '","maxDepth":60}')
    & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Observe -ArgsFile $a | Out-Null
    Start-Sleep -Milliseconds 1500
    [System.IO.File]::WriteAllText($a, '{"ref":"' + $win + '","maxDepth":60,"bIncludeSourceLocations":false}')
    & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Snapshot -ArgsFile $a -Out $snap | Out-Null
    $t = [System.IO.File]::ReadAllText($snap)

    if ($try -eq 0) {
        $m = [regex]::Match($t, 'button \\"현재 레벨 저장\\" \[[^\]]*\] \[ref=(\w+)\]')
        if ($m.Success) { Click-Ref $m.Groups[1].Value }
    }
    else {
        # 레벨 탭을 눌러 레벨 편집기에 포커스를 준 뒤 Ctrl+S
        $name = $lvl.Split('/')[-1]
        $tab = [regex]::Match($t, 'tab \\"' + $name + '\\" \[[^\]]*\] \[ref=(\w+)\]')
        if ($tab.Success) { Click-Ref $tab.Groups[1].Value }
        [System.IO.File]::WriteAllText($a, '{"key":"Ctrl+S"}')
        & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool PressKey -ArgsFile $a | Out-Null
    }
    Start-Sleep -Seconds 6
}
Restore-Min
"new files: " + (Get-NewCount)
