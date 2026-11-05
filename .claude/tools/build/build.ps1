param(
    [string]$Target = "EclipseTheNinjaEditor",
    [string]$Config = "Development",
    [int]$Tail = 40
)
# UBT 빌드 후 에러·경고만 추려서 출력한다. 전체 로그는 %TEMP%\EclipseBuild\build_last.log
# 에디터가 이 프로젝트를 열고 있으면 링크가 실패하므로 먼저 editor.ps1 -Action close 를 거친다
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$uproject = Join-Path $root "EclipseTheNinja.uproject"
$buildBat = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$work = Join-Path $env:TEMP "EclipseBuild"
New-Item -ItemType Directory -Force $work | Out-Null
$log = Join-Path $work "build_last.log"

$running = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { (Get-CimInstance Win32_Process -Filter "ProcessId=$($_.Id)").CommandLine -match 'EclipseTheNinja.uproject' }
if ($running -and $Target -like "*Editor") {
    "refused: 에디터가 실행 중 (pid $($running.Id)). editor.ps1 -Action close 먼저"
    exit 2
}

$sw = [Diagnostics.Stopwatch]::StartNew()
& cmd /c "`"$buildBat`" $Target Win64 $Config -Project=`"$uproject`" -WaitMutex -NoHotReloadFromIDE > `"$log`" 2>&1"
$code = $LASTEXITCODE
$sw.Stop()

$lines = Get-Content $log -Encoding UTF8
$errors = @($lines | Where-Object { $_ -match '\berror\b|: fatal|LNK\d{4}|C\d{4}:' -and $_ -notmatch 'warning' })
$warnings = @($lines | Where-Object { $_ -match '\bwarning\b' -and $_ -match '\\Source\\' })

"exit=$code time=$([int]$sw.Elapsed.TotalSeconds)s errors=$($errors.Count) project-warnings=$($warnings.Count)"
if ($errors.Count -gt 0) {
    "--- errors (중복 제거, 최대 30줄) ---"
    $errors | Select-Object -Unique | Select-Object -First 30
}
if ($warnings.Count -gt 0) {
    "--- project warnings (최대 15줄) ---"
    $warnings | Select-Object -Unique | Select-Object -First 15
}
if ($code -ne 0 -and $errors.Count -eq 0) {
    "--- log tail ---"
    $lines | Select-Object -Last $Tail
}
"log: $log"

# 전체 빌드가 성공하면 Live Coding 으로만 반영돼 있던 변경도 DLL 에 들어갔으므로 표시를 지운다
if ($code -eq 0) {
    Remove-Item (Join-Path $work "livecoding_pending.txt") -ErrorAction SilentlyContinue
}

exit $code
