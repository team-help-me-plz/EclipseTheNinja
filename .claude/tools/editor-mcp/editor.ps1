param(
    [Parameter(Mandatory=$true)][ValidateSet("status", "unsaved", "close", "launch", "livecode")][string]$Action,
    [int]$WaitSec = 180,
    [switch]$PromptOnUnsaved
)
# 에디터 프로세스 관리
#   status  : 실행 여부와 MCP 응답 여부
#   unsaved : 미저장 개수와 목록을 낸다. 로컬 플러그인 EclipseLocalTools 의 GetDirtyPackages 로 정확히 센다
#             플러그인이 없으면 상태 표시줄의 "N개 저장되지 않음" 글자를 읽는다 (버튼은 절대 클릭하지 않는다 - 모달이 뜨면 MCP가 멈춘다)
#             글자를 못 읽으면 창을 앞으로 가져와 한 번 더 읽는다
#   livecode: 에디터를 켠 채 Live Coding 으로 .cpp 변경분을 컴파일한다 (헤더·UPROPERTY 변경은 close + build.ps1 로 전체 빌드)
#   close   : 미저장이 0일 때만 CloseMainWindow 로 정상 종료한다. 강제 종료는 하지 않는다
#             -PromptOnUnsaved 면 미저장이 있거나 판별 불가여도 닫기를 요청한다. 미저장이 있으면 에디터가 직접
#             "저장할까요?" 창을 띄우고, 사용자가 고를 때까지 WaitSec 초 기다린다 (MCP는 저장 목록을 주지 않는다)
#   launch  : 에디터를 켜고 MCP가 응답할 때까지 기다린다
$s = $PSScriptRoot
$work = Join-Path $env:TEMP "EclipseMcp"
New-Item -ItemType Directory -Force $work | Out-Null
$call = Join-Path $s "call.ps1"
$root = (Resolve-Path (Join-Path $s "..\..\..")).Path
$uproject = Join-Path $root "EclipseTheNinja.uproject"
$editorExe = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

Add-Type @"
using System; using System.Runtime.InteropServices;
public class WinE { [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h); [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h); }
"@

function Get-Editor {
    return Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { (Get-CimInstance Win32_Process -Filter "ProcessId=$($_.Id)").CommandLine -match 'EclipseTheNinja.uproject' } | Select-Object -First 1
}
function Test-Mcp {
    $r = & $call -Toolset editor_toolset.toolsets.scene.SceneTools -Tool get_current_level -Timeout 15
    return (($r -join '') -match 'returnValue')
}

# 반환값 : 0 이상 = 미저장 개수, -1 = 판별 불가
# 포커스 없이 읽어 보고, 못 읽으면 창을 앞으로 가져와 한 번 더 읽는다 (가려진 창은 Slate 트리가 비어 있을 때가 있다)
$script:DirtyNames = @()
$localToolset = "EclipseLocalTools.EclipseLocalToolset"

# Live Coding 으로만 반영된 변경이 있다는 표시. build.ps1 이 성공하면 지운다
$liveCodingMarker = Join-Path $env:TEMP "EclipseBuild\livecoding_pending.txt"

function Get-Unsaved {
    $ed = Get-Editor
    if (-not $ed) {
        return -1
    }

    # 로컬 플러그인이 있으면 엔진이 아는 미저장 패키지를 그대로 센다
    $r = (& $call -Toolset $localToolset -Tool GetDirtyPackages -Timeout 30) -join ""
    if ($r -match '"returnValue"') {
        try {
            $script:DirtyNames = @(($r | ConvertFrom-Json).returnValue)
            return $script:DirtyNames.Count
        }
        catch {
        }
    }

    $n = Read-Unsaved $ed $false
    if ($n -lt 0) {
        $n = Read-Unsaved $ed $true
    }

    return $n
}

function Read-Unsaved($ed, [bool]$bringToFront) {
    # 최소화된 창은 Slate가 보지 못하므로 잠깐 복원한다. 재시도 때는 앞으로 가져온다
    $wasIconic = [WinE]::IsIconic($ed.MainWindowHandle)
    if ($bringToFront) {
        [WinE]::ShowWindow($ed.MainWindowHandle, 9) | Out-Null
        [WinE]::SetForegroundWindow($ed.MainWindowHandle) | Out-Null
        Start-Sleep -Seconds 1
    }
    elseif ($wasIconic) {
        [WinE]::ShowWindow($ed.MainWindowHandle, 4) | Out-Null
        Start-Sleep -Seconds 1
    }

    $a = Join-Path $work "a_unsaved.json"
    $snap = Join-Path $work "snap_unsaved.txt"
    [IO.File]::WriteAllText($a, '{"ref":"","maxDepth":0}')
    & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Observe -ArgsFile $a | Out-Null
    [IO.File]::WriteAllText($a, '{"ref":"","maxDepth":1,"bIncludeSourceLocations":false}')
    & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Snapshot -ArgsFile $a -Out $snap | Out-Null
    $win = [regex]::Match([IO.File]::ReadAllText($snap), 'window \\"EclipseTheNinja[^\[]*\[[^\]]*\] \[ref=(\w+)\]').Groups[1].Value
    $n = -1
    if ($win -ne "") {
        [IO.File]::WriteAllText($a, '{"ref":"' + $win + '","maxDepth":60}')
        & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Observe -ArgsFile $a | Out-Null
        Start-Sleep -Milliseconds 1500
        [IO.File]::WriteAllText($a, '{"ref":"' + $win + '","maxDepth":60,"bIncludeSourceLocations":false}')
        & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Snapshot -ArgsFile $a -Out $snap | Out-Null
        $t = [IO.File]::ReadAllText($snap)
        $m = [regex]::Match($t, '(\d+)\s*개 저장되지 않음')
        if ($m.Success) {
            $n = [int]$m.Groups[1].Value
        }
        elseif ($t -match '현재 레벨 저장') {
            # 툴바는 보이는데 미저장 표시가 없으면 0개로 본다
            $n = 0
        }
    }

    if ($wasIconic) {
        [WinE]::ShowWindow($ed.MainWindowHandle, 7) | Out-Null
    }

    return $n
}

switch ($Action) {
    "status" {
        $ed = Get-Editor
        if (-not $ed) {
            "running=false"
        }
        else {
            "running=true pid=$($ed.Id) mcp=$(Test-Mcp)"
        }
    }
    "unsaved" {
        "unsaved=" + (Get-Unsaved)
        $script:DirtyNames | Select-Object -First 30 | ForEach-Object { "  " + $_ }
    }
    "livecode" {
        if (-not (Get-Editor)) {
            "livecode: 에디터가 꺼져 있음. build.ps1 로 빌드할 것"
            exit 2
        }

        # 컴파일이 끝날 때까지 에디터가 기다린다. 실패하면 LogLiveCoding 에서 에러 줄만 추린다
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $r = (& $call -Toolset $localToolset -Tool LiveCodingCompile -Timeout 900) -join ""
        $result = [regex]::Match($r, '"returnValue"\s*:\s*"(\w+)"').Groups[1].Value
        if ($result -eq "") {
            "livecode: 호출 실패 (플러그인이 없거나 MCP 오류) " + $r.Substring(0, [Math]::Min(300, $r.Length))
            exit 3
        }

        "livecode=$result time=$([int]$sw.Elapsed.TotalSeconds)s"

        # Live Coding 패치는 에디터를 다시 켜면 사라진다. build.ps1 이 성공할 때까지 표시를 남겨 launch 가 막도록 한다
        if ($result -eq "Success") {
            New-Item -ItemType Directory -Force (Split-Path $liveCodingMarker) | Out-Null
            Set-Content -Path $liveCodingMarker -Value (Get-Date -Format s)
        }
        if ($result -eq "Failure") {
            $a = Join-Path $work "a_lc_log.json"
            [IO.File]::WriteAllText($a, '{"category":"LogLiveCoding","pattern":"error|Error|warning C|LNK","maxEntries":40}')
            $log = (& $call -Toolset EditorToolset.LogsToolset -Tool GetLogEntries -ArgsFile $a) -join ""
            try {
                ($log | ConvertFrom-Json).returnValue | Select-Object -Unique | Select-Object -Last 30
            }
            catch {
                $log.Substring(0, [Math]::Min(2000, $log.Length))
            }
            exit 1
        }

        if ($result -ne "Success" -and $result -ne "NoChanges") {
            exit 1
        }

        exit 0
    }
    "close" {
        $ed = Get-Editor
        if (-not $ed) {
            "closed (was not running)"
            exit 0
        }

        $n = Get-Unsaved
        if ($n -ne 0 -and -not $PromptOnUnsaved) {
            "refused: unsaved=$n (-1 = 판별 불가). 사용자에게 저장을 요청할 것"
            exit 2
        }

        # 미저장이 있으면 에디터가 스스로 저장 여부를 묻는다. 고르는 건 사용자이고 여기서는 기다리기만 한다
        if ($n -ne 0) {
            "prompting: unsaved=$n (-1 = 판별 불가). 에디터의 저장 확인 창에서 사용자가 고를 때까지 최대 $WaitSec 초 기다림"
            $script:DirtyNames | Select-Object -First 30 | ForEach-Object { "  " + $_ }
        }

        $ed.CloseMainWindow() | Out-Null
        if ($ed.WaitForExit($WaitSec * 1000)) {
            "closed"
            exit 0
        }

        "timeout: 에디터가 $WaitSec 초 안에 종료되지 않음 (저장 확인 창에서 아직 고르지 않았거나 취소함). 강제 종료하지 않았음"
        exit 3
    }
    "launch" {
        if (Get-Editor) {
            "already running"
            exit 0
        }

        # 그대로 켜면 디스크의 옛 DLL 이 올라와 Live Coding 으로 고친 코드가 사라진다
        if (Test-Path $liveCodingMarker) {
            "refused: Live Coding 으로만 반영된 변경이 있음 ($([IO.File]::ReadAllText($liveCodingMarker).Trim())). build.ps1 로 전체 빌드를 먼저 할 것"
            exit 5
        }

        Start-Process -FilePath $editorExe -ArgumentList ('"' + $uproject + '"') | Out-Null
        $deadline = (Get-Date).AddSeconds($WaitSec)
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Seconds 10
            Remove-Item (Join-Path $work "mcp_sid.txt") -ErrorAction SilentlyContinue
            if (Test-Mcp) {
                "launched, mcp ready"
                exit 0
            }
        }

        "launched, but mcp not ready after $WaitSec s"
        exit 4
    }
}
