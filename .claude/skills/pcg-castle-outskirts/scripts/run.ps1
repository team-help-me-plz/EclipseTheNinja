param(
    [Parameter(Mandatory=$true)][ValidateSet('inspect','apply','regen','count','shots','save')][string]$Mode,
    [string]$Layout = "",
    [string[]]$Labels = @(),
    [string]$Shots = "",
    [string]$OutDir = ""
)
# pcg_build.py를 실행 중인 에디터에서 돌린다 (EclipseLocalTools ExecuteConsoleCommand → py)
# 예: run.ps1 -Mode apply -Layout <레이아웃.json>
#     run.ps1 -Mode shots -Shots "Preview_GameCam_A=a.png,Preview_GameCam_B=b.png" [-OutDir <폴더>]
$ErrorActionPreference = 'Stop'
# powershell -File 로 부르면 "A,B"가 문자열 하나로 들어오므로 쉼표로 나눈다
$Labels = @($Labels | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path
$call = Join-Path $root ".claude\tools\editor-mcp\call.ps1"
$tmp = Join-Path $env:TEMP "EclipseMcp"
New-Item -ItemType Directory -Force $tmp | Out-Null
$enc = New-Object System.Text.UTF8Encoding($false)
$py = (Join-Path $PSScriptRoot "pcg_build.py").Replace('\', '/')
$out = Join-Path $tmp "pcg_build.out.txt"

# 인자 파일을 쓰고 py를 한 번 실행한 뒤 로그를 돌려준다
function Invoke-Build($argObj) {
    [IO.File]::WriteAllText((Join-Path $tmp "pcg_build_args.json"), ($argObj | ConvertTo-Json -Depth 6 -Compress), $enc)
    if (Test-Path $out) { Remove-Item $out }
    $cf = Join-Path $tmp ("pcg_con_" + [guid]::NewGuid().ToString("N") + ".json")
    [IO.File]::WriteAllText($cf, (@{ Command = "py `"$py`"" } | ConvertTo-Json -Compress), $enc)
    & $call -Toolset EclipseLocalTools.EclipseLocalToolset -Tool ExecuteConsoleCommand -ArgsFile $cf | Out-Null
    Remove-Item $cf -ErrorAction SilentlyContinue
    if (Test-Path $out) { Get-Content $out -Encoding UTF8 } else { "!! 출력 없음: 에디터/MCP 상태 확인 (editor.ps1 -Action status)" }
}

if ($Mode -eq 'shots') {
    # 경로는 프로젝트 루트 기준 절대 경로로 바꿔 넘긴다(에디터 작업 폴더는 엔진 폴더라 상대 경로가 엉뚱한 곳에 저장됨)
    if (-not $OutDir) { $OutDir = "Saved\PCGReview\Skill" }
    if (-not [IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path $root $OutDir }
    New-Item -ItemType Directory -Force $OutDir | Out-Null

    # 스크린샷은 에디터 창이 앞에 있어야 렌더된다
    Add-Type @"
using System; using System.Runtime.InteropServices;
public class PcgWin { [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h); }
"@
    $ed = Get-Process UnrealEditor | Select-Object -First 1
    [PcgWin]::ShowWindow($ed.MainWindowHandle, 9) | Out-Null
    [PcgWin]::SetForegroundWindow($ed.MainWindowHandle) | Out-Null
    Start-Sleep 2

    # 카메라마다 따로 요청하고 파일이 생길 때까지 기다린다
    foreach ($kv in $Shots.Split(',')) {
        $p = $kv.Split('=')
        $f = Join-Path $OutDir $p[1].Trim()
        if (Test-Path $f) { Remove-Item $f }
        Invoke-Build @{ mode = 'shots'; shot = @($p[0].Trim(), $f) }
        for ($i = 0; $i -lt 40; $i++) { if (Test-Path $f) { break }; Start-Sleep 1 }
        if (Test-Path $f) { "ok $f" } else { "MISSING $f (에디터 창이 앞에 있는지 확인)" }
    }
    return
}

$a = @{ mode = $Mode }
if ($Layout) { $a.layout = (Resolve-Path $Layout).Path }
if ($Labels.Count -gt 0) { $a.labels = $Labels }
Invoke-Build $a

# 생성은 비동기라 잠시 뒤 따로 센다
if ($Mode -eq 'apply' -or $Mode -eq 'regen') {
    Start-Sleep 3
    "--- 생성 결과"
    $c = @{ mode = 'count' }
    if ($Labels.Count -gt 0) { $c.labels = $Labels }
    Invoke-Build $c
}
