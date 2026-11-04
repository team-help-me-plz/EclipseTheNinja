param(
    [Parameter(Mandatory=$true)][string]$Body,
    [switch]$Kit,
    [string]$Out = ""
)
# 샌드박스는 다른 파일을 import 하지 못하므로 core.py (+ -Kit 이면 level_kit.py) 를 작업 스크립트 앞에 이어 붙여 실행한다
$s = $PSScriptRoot
$work = Join-Path $env:TEMP "EclipseMcp"
New-Item -ItemType Directory -Force $work | Out-Null
$parts = @([System.IO.File]::ReadAllText((Join-Path $s "core.py"), [System.Text.Encoding]::UTF8))
if ($Kit) { $parts += [System.IO.File]::ReadAllText((Join-Path $s "level_kit.py"), [System.Text.Encoding]::UTF8) }
$parts += [System.IO.File]::ReadAllText($Body, [System.Text.Encoding]::UTF8)
$f = Join-Path $work ("combined_" + [guid]::NewGuid().ToString("N") + ".py")
[System.IO.File]::WriteAllText($f, ($parts -join "`n`n"), (New-Object System.Text.UTF8Encoding($false)))
& (Join-Path $s "py.ps1") -Script $f -Out $Out
Remove-Item $f -ErrorAction SilentlyContinue
