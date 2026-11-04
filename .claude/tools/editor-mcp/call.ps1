param(
    [string]$Toolset = "",
    [Parameter(Mandatory=$true)][string]$Tool,
    [string]$ArgsFile = "",
    [string]$ArgsJson = "",
    [string]$Out = "",
    [int]$Timeout = 600
)
# call_tool 요청을 감싸서 결과 텍스트만 출력한다. 실패는 RPCERR:/TOOLERR: 접두어로 구분한다
# 예) call.ps1 -Tool describe_toolset -ArgsJson '{"toolset_name":"editor_toolset.toolsets.scene.SceneTools"}'
$work = Join-Path $env:TEMP "EclipseMcp"
New-Item -ItemType Directory -Force $work | Out-Null
$argsText = "{}"
if ($ArgsFile -ne "") { $argsText = [IO.File]::ReadAllText($ArgsFile, [Text.Encoding]::UTF8) }
elseif ($ArgsJson -ne "") { $argsText = $ArgsJson }
$o = '{"name":"call_tool","arguments":{'
if ($Toolset -ne "") { $o += '"toolset_name":"' + $Toolset + '",' }
$o += '"tool_name":"' + $Tool + '","arguments":' + $argsText + '}}'

# 동시에 여러 호출이 돌아도 겹치지 않게 요청 파일을 호출마다 따로 만든다
$pf = Join-Path $work ("p_call_" + [guid]::NewGuid().ToString("N") + ".json")
[System.IO.File]::WriteAllText($pf, $o, (New-Object System.Text.UTF8Encoding($false)))
$raw = & (Join-Path $PSScriptRoot "mcp.ps1") -Method tools/call -ParamsFile $pf -Timeout $Timeout
Remove-Item $pf -ErrorAction SilentlyContinue
$raw = $raw -join "`n"
try {
    $j = $raw | ConvertFrom-Json
    if ($j.error) { $text = "RPCERR: " + ($j.error | ConvertTo-Json -Depth 5) }
    else { $text = ($j.result.content | % { $_.text }) -join "`n"; if ($j.result.isError) { $text = "TOOLERR: " + $text } }
} catch { $text = $raw }
if ($Out -ne "") { [System.IO.File]::WriteAllText($Out, $text, (New-Object System.Text.UTF8Encoding($false))); "written " + $text.Length }
else { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8; $text }
