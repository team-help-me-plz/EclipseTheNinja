param(
    [Parameter(Mandatory=$true)][ValidateSet("start", "stop", "tp", "loc", "shots", "watch")][string]$Action,
    [string]$Map = "",
    [string]$Actor = "BP_Week_2_Character_C_0",
    [double]$X = 0, [double]$Y = 1000, [double]$Z = 0, [double]$Yaw = 0,
    [string]$Points = "",
    [string]$OutDir = "",
    [string]$Prefix = "shot",
    [switch]$Teleport,
    [int]$Count = 8,
    [double]$Interval = 1.0,
    [double]$Warmup = 2.0
)
# PIE 보조 : 시작/정지, PIE 액터 순간이동·위치 읽기, 지점별 게임 화면 캡처, 한 자리에서 시간 순서 캡처
# -Map 은 레벨 에셋 경로 (예: /Game/Maps/Week_3/Lvl_Week_3_Prototype_1). PIE 액터 경로는 UEDPIE_0_<맵> 으로 만든다
# 플레이어 입력(키·능력)은 MCP로 넣을 수 없다. 조작이 필요한 테스트는 사용자에게 가이드로 넘긴다
# shots 예) pie.ps1 -Action shots -Map /Game/Maps/Week_3/Lvl_X -Points "0:200;1500:400" -OutDir <scratchpad>
# watch 예) pie.ps1 -Action watch -Map /Game/Maps/Week_3/Lvl_X -Teleport -X 2300 -Z 100 -Count 8 -Interval 1 -OutDir <scratchpad>
#   PIE를 켜고(-Teleport 면 플레이어를 X,Y,Z로 옮긴 뒤) Warmup 초 기다렸다가 Interval 초마다 Count 장 찍고 PIE를 끈다
#   플레이어가 가만히 서 있는 동안 AI가 어떻게 움직이는지처럼 조작 없이 지켜보면 되는 장면에 쓴다
#   ffmpeg가 있으면 <Prefix>_sheet.png 로 한 장에 이어 붙인다 (이미지 여러 장보다 읽기 싸다)
$s = $PSScriptRoot
$work = Join-Path $env:TEMP "EclipseMcp"
New-Item -ItemType Directory -Force $work | Out-Null
$call = Join-Path $s "call.ps1"
$inv = [System.Globalization.CultureInfo]::InvariantCulture
$a = Join-Path $work ("a_pie_" + [guid]::NewGuid().ToString("N") + ".json")

function Get-PiePath([string]$actorName) {
    $dir = $Map.Substring(0, $Map.LastIndexOf('/'))
    $name = $Map.Split('/')[-1]
    return "$dir/UEDPIE_0_$name.$name" + ":PersistentLevel." + $actorName
}
function Teleport([double]$px, [double]$py, [double]$pz, [double]$pyaw) {
    $j = '{"actor":{"refPath":"' + (Get-PiePath $Actor) + '"},"xform":{"location":{"x":' + $px.ToString($inv) + ',"y":' + $py.ToString($inv) + ',"z":' + $pz.ToString($inv) + '},"rotation":{"pitch":0,"yaw":' + $pyaw.ToString($inv) + ',"roll":0}},"worldspace":true}'
    [IO.File]::WriteAllText($a, $j)
    & $call -Toolset editor_toolset.toolsets.actor.ActorTools -Tool set_actor_transform -ArgsFile $a
}
function Start-Pie {
    [IO.File]::WriteAllText($a, '{"options":{"bSimulate":false,"playMode":"PlayMode_InViewPort","warmupSeconds":3}}')
    & $call -Toolset EditorToolset.EditorAppToolset -Tool StartPIE -ArgsFile $a
}
# 메인 창(w1)의 Slate 스크린샷을 PNG로 저장한다. 실패하면 응답 앞부분을 돌려준다
function Save-Shot([string]$path) {
    [IO.File]::WriteAllText($a, '{"ref":"w1"}')
    $tmp = Join-Path $work ("ss_" + [guid]::NewGuid().ToString("N") + ".txt")
    & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Screenshot -ArgsFile $a -Out $tmp | Out-Null
    $t = [IO.File]::ReadAllText($tmp)
    Remove-Item $tmp -ErrorAction SilentlyContinue
    $m = [regex]::Match($t, '"data"\s*:\s*"([^"]+)"')
    if ($m.Success) {
        [IO.File]::WriteAllBytes($path, [Convert]::FromBase64String($m.Groups[1].Value))
        return ""
    }

    return $t.Substring(0, [Math]::Min(300, $t.Length))
}

switch ($Action) {
    "start" { Start-Pie }
    "stop" { & $call -Toolset EditorToolset.EditorAppToolset -Tool StopPIE }
    "tp" { Teleport $X $Y $Z $Yaw }
    "loc" {
        [IO.File]::WriteAllText($a, '{"actor":{"refPath":"' + (Get-PiePath $Actor) + '"}}')
        & $call -Toolset editor_toolset.toolsets.actor.ActorTools -Tool get_actor_transform -ArgsFile $a
    }
    "shots" {
        # Points 는 "x:z;x:z" 형식. 각 지점으로 옮긴 뒤 스트리밍을 기다렸다가 Slate 스크린샷(w1 = 메인 창)을 찍는다
        Start-Pie | Out-Null
        Start-Sleep -Seconds 2
        $i = 0
        foreach ($p in $Points.Split(';')) {
            $xz = $p.Split(':')
            Teleport ([double]::Parse($xz[0], $inv)) $Y ([double]::Parse($xz[1], $inv)) 0 | Out-Null
            Start-Sleep -Milliseconds 2500
            [IO.File]::WriteAllText($a, '{"ref":"w1"}')
            $tmp = Join-Path $work "ss_tmp.txt"
            & $call -Toolset SlateInspectorToolset.SlateInspectorToolset -Tool Screenshot -ArgsFile $a -Out $tmp | Out-Null
            $t = [IO.File]::ReadAllText($tmp)
            $m = [regex]::Match($t, '"data"\s*:\s*"([^"]+)"')
            if ($m.Success) { [IO.File]::WriteAllBytes((Join-Path $OutDir ($Prefix + "_" + $i + ".png")), [Convert]::FromBase64String($m.Groups[1].Value)) } else { $t.Substring(0, [Math]::Min(300, $t.Length)) }
            $i++
        }
        & $call -Toolset EditorToolset.EditorAppToolset -Tool StopPIE | Out-Null
        "shots $i"
    }
    "watch" {
        if ($OutDir -eq "") {
            "watch: -OutDir 가 필요함"
            break
        }

        New-Item -ItemType Directory -Force $OutDir | Out-Null
        Start-Pie | Out-Null
        Start-Sleep -Seconds 2
        if ($Teleport) {
            Teleport $X $Y $Z $Yaw | Out-Null
        }

        Start-Sleep -Milliseconds ([int]($Warmup * 1000))
        $saved = 0
        $sw = [Diagnostics.Stopwatch]::StartNew()
        for ($i = 0; $i -lt $Count; $i++) {
            # 스크린샷 자체에 걸린 시간을 빼고 기다려 간격을 일정하게 맞춘다
            $due = [int]($i * $Interval * 1000)
            $wait = $due - [int]$sw.ElapsedMilliseconds
            if ($wait -gt 0) {
                Start-Sleep -Milliseconds $wait
            }

            $err = Save-Shot (Join-Path $OutDir ("{0}_{1:D2}.png" -f $Prefix, $i))
            if ($err -ne "") {
                "shot $i 실패: $err"
                continue
            }

            $saved++
        }

        & $call -Toolset EditorToolset.EditorAppToolset -Tool StopPIE | Out-Null
        "watch $saved/$Count (간격 $Interval 초, 예열 $Warmup 초)"

        # 여러 장을 한 장으로 이어 붙인다. 4열, 한 칸 너비 640
        $ff = Get-Command ffmpeg -ErrorAction SilentlyContinue
        if ($ff -and $saved -gt 0) {
            $rows = [Math]::Ceiling($saved / 4.0)
            $sheet = Join-Path $OutDir ($Prefix + "_sheet.png")
            & $ff.Source -v error -y -framerate 1 -i (Join-Path $OutDir ($Prefix + "_%02d.png")) -vf ("scale=640:-1,tile=4x" + $rows) -frames:v 1 $sheet
            if (Test-Path $sheet) {
                "sheet: $sheet"
            }
        }
    }
}
Remove-Item $a -ErrorAction SilentlyContinue
