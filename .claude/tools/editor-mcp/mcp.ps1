param(
    [Parameter(Mandatory=$true)][string]$Method,
    [string]$ParamsFile = "",
    [string]$ParamsJson = "{}",
    [int]$Timeout = 300
)
# 언리얼 에디터 MCP(localhost:8000) 최소 클라이언트. 세션 ID는 작업 폴더에 캐시하고 끊기면 한 번 재연결한다
$work = Join-Path $env:TEMP "EclipseMcp"
New-Item -ItemType Directory -Force $work | Out-Null
$sidFile = Join-Path $work "mcp_sid.txt"
$h = @{ Accept = 'application/json, text/event-stream' }

function Init {
    $body = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"claude","version":"1"}}}'
    try {
        $r = Invoke-WebRequest -Uri http://localhost:8000/mcp -Method Post -Body $body -ContentType 'application/json' -Headers $h -UseBasicParsing -TimeoutSec 20
    }
    catch {
        # 에디터가 꺼져 있거나 아직 MCP가 뜨지 않음
        [Console]::Out.WriteLine("ERR: editor MCP not reachable (localhost:8000)")
        exit 1
    }
    $sid = $r.Headers['Mcp-Session-Id']
    Set-Content -Path $sidFile -Value $sid -NoNewline
    $h2 = $h.Clone(); $h2['Mcp-Session-Id'] = $sid
    try { Invoke-WebRequest -Uri http://localhost:8000/mcp -Method Post -Body '{"jsonrpc":"2.0","method":"notifications/initialized"}' -ContentType 'application/json' -Headers $h2 -UseBasicParsing -TimeoutSec 20 | Out-Null } catch {}
    return $sid
}

if ((Test-Path $sidFile) -and ((Get-Item $sidFile).Length -gt 0)) { $sid = Get-Content $sidFile -Raw } else { $sid = Init }

if ($ParamsFile -ne "") { $params = [IO.File]::ReadAllText($ParamsFile, [Text.Encoding]::UTF8) } else { $params = $ParamsJson }
$body = '{"jsonrpc":"2.0","id":2,"method":"' + $Method + '","params":' + $params + '}'
$bytes = [System.Text.Encoding]::UTF8.GetBytes($body)

for ($i = 0; $i -lt 2; $i++) {
    $h2 = $h.Clone(); $h2['Mcp-Session-Id'] = $sid
    try {
        $r = Invoke-WebRequest -Uri http://localhost:8000/mcp -Method Post -Body $bytes -ContentType 'application/json; charset=utf-8' -Headers $h2 -UseBasicParsing -TimeoutSec $Timeout
        $content = [System.Text.Encoding]::UTF8.GetString($r.RawContentStream.ToArray())
        [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
        Write-Output $content
        break
    } catch {
        if ($i -eq 0) { $sid = Init } else { Write-Output "ERR: $_" }
    }
}
