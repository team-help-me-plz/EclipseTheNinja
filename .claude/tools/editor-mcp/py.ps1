param(
    [Parameter(Mandatory=$true)][string]$Script,
    [string]$Out = ""
)
# 파이썬 파일 하나를 ProgrammaticToolset.execute_tool_script 샌드박스에서 실행한다 (def run(): 이 dict를 반환해야 함)
$work = Join-Path $env:TEMP "EclipseMcp"
New-Item -ItemType Directory -Force $work | Out-Null
$code = [System.IO.File]::ReadAllText($Script, [System.Text.Encoding]::UTF8)
$argsJson = @{ script = $code } | ConvertTo-Json -Compress
$af = Join-Path $work ("p_py_" + [guid]::NewGuid().ToString("N") + ".json")
[System.IO.File]::WriteAllText($af, $argsJson, (New-Object System.Text.UTF8Encoding($false)))
& (Join-Path $PSScriptRoot "call.ps1") -Toolset editor_toolset.toolsets.programmatic.ProgrammaticToolset -Tool execute_tool_script -ArgsFile $af -Out $Out
Remove-Item $af -ErrorAction SilentlyContinue
