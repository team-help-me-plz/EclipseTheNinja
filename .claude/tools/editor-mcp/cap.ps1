param(
    [double]$X, [double]$Y, [double]$Z,
    [double]$Yaw = -90, [double]$Pitch = 0,
    [Parameter(Mandatory=$true)][string]$Out
)
# 에디터 카메라를 지정 위치에 두고 뷰포트를 PNG로 저장한다 (PIE 화면은 pie.ps1 -Action shots)
$work = Join-Path $env:TEMP "EclipseMcp"
New-Item -ItemType Directory -Force $work | Out-Null
$a = @{ captureTransform = @{ location = @{ x = $X; y = $Y; z = $Z }; rotation = @{ pitch = $Pitch; yaw = $Yaw; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } }; bShowUI = $false; annotations = @{ gridSpacing = 0; gridExtent = 0; gridHeight = 0 } } | ConvertTo-Json -Depth 10 -Compress
$tmp = Join-Path $work 'cap_args.json'
[System.IO.File]::WriteAllText($tmp, $a)
$txt = & (Join-Path $PSScriptRoot 'call.ps1') -Toolset 'EditorToolset.EditorAppToolset' -Tool 'CaptureViewport' -ArgsFile $tmp | Out-String
try { $j = $txt | ConvertFrom-Json; $rv = $j; if ($j.returnValue) { $rv = $j.returnValue }; [System.IO.File]::WriteAllBytes($Out, [Convert]::FromBase64String($rv.image.data)); "saved $Out" } catch { $txt.Substring(0, [Math]::Min(800, $txt.Length)) }
