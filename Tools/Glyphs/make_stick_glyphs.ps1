param([string]$OutDir)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'

$Size = 256
$White = [Drawing.Color]::FromArgb(255, 255, 255, 255)
$Black = [Drawing.Color]::FromArgb(255, 8, 11, 14)

# 원본 GP_LeftStick 비율: 원판 반지름 120.5, 링 바깥 0.867R, 링 안쪽 0.784R, 글자 높이 0.83R
$DiscR = 92.0
$Offset = 26.0
$ArrowTip = 9.0
$ArrowDepth = 46.0
$ArrowHalf = 34.0

function New-Glyph([string]$Letter, [string]$Dir)
{
    $bmp = New-Object Drawing.Bitmap $Size, $Size, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.PixelOffsetMode = 'HighQuality'
    $g.CompositingQuality = 'HighQuality'
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $g.Clear([Drawing.Color]::Transparent)

    # 방향 벡터(화면 좌표, y는 아래가 +)
    switch ($Dir)
    {
        'Up'    { $dx = 0;  $dy = -1 }
        'Down'  { $dx = 0;  $dy = 1 }
        'Left'  { $dx = -1; $dy = 0 }
        'Right' { $dx = 1;  $dy = 0 }
    }

    $c = $Size / 2.0
    $cx = $c - $dx * $Offset
    $cy = $c - $dy * $Offset

    # 원판 + 링
    $wb = New-Object Drawing.SolidBrush $White
    $bb = New-Object Drawing.SolidBrush $Black
    $g.FillEllipse($wb, [float]($cx - $DiscR), [float]($cy - $DiscR), [float]($DiscR * 2), [float]($DiscR * 2))

    $ringOuter = $DiscR * 0.867
    $ringInner = $DiscR * 0.784
    $ringMid = ($ringOuter + $ringInner) / 2.0
    $pen = New-Object Drawing.Pen $Black, ([float]($ringOuter - $ringInner))
    $g.DrawEllipse($pen, [float]($cx - $ringMid), [float]($cy - $ringMid), [float]($ringMid * 2), [float]($ringMid * 2))

    # 글자: 외곽선 경로로 만든 뒤 실제 글자 영역 기준으로 원판 가운데에 맞춘다
    $capH = $DiscR * 0.83
    $path = New-Object Drawing.Drawing2D.GraphicsPath
    $family = New-Object Drawing.FontFamily 'Arial'
    $path.AddString($Letter, $family, [int][Drawing.FontStyle]::Bold, [float]200, (New-Object Drawing.PointF 0, 0), [Drawing.StringFormat]::GenericTypographic)
    $bounds = $path.GetBounds()
    $scale = $capH / $bounds.Height
    $m = New-Object Drawing.Drawing2D.Matrix
    $m.Translate([float]$cx, [float]$cy)
    $m.Scale([float]$scale, [float]$scale)
    $m.Translate([float](-($bounds.X + $bounds.Width / 2.0)), [float](-($bounds.Y + $bounds.Height / 2.0)))
    $path.Transform($m)
    $g.FillPath($bb, $path)

    # 방향 화살표: 원판 반대쪽 빈 공간에 흰 삼각형
    $tx = $c + $dx * ($c - $ArrowTip)
    $ty = $c + $dy * ($c - $ArrowTip)
    $bx = $tx - $dx * $ArrowDepth
    $by = $ty - $dy * $ArrowDepth
    $px = -$dy
    $py = $dx
    $pts = @(
        (New-Object Drawing.PointF ([float]$tx), ([float]$ty)),
        (New-Object Drawing.PointF ([float]($bx + $px * $ArrowHalf)), ([float]($by + $py * $ArrowHalf))),
        (New-Object Drawing.PointF ([float]($bx - $px * $ArrowHalf)), ([float]($by - $py * $ArrowHalf)))
    )
    $g.FillPolygon($wb, [Drawing.PointF[]]$pts)

    $g.Dispose()
    return $bmp
}

$names = @()
foreach ($side in @(@('L', 'LeftStick'), @('R', 'RightStick')))
{
    foreach ($dir in 'Up', 'Down', 'Left', 'Right')
    {
        $bmp = New-Glyph $side[0] $dir
        $name = "GP_$($side[1])_$dir"
        $bmp.Save((Join-Path $OutDir "$name.png"), [Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        $names += $name
    }
}

# 미리보기: 기존 스틱 아이콘과 나란히, 어두운 배경 + 밝은 배경
$cell = 130
$sheet = New-Object Drawing.Bitmap ($cell * 5), ($cell * 4)
$g = [Drawing.Graphics]::FromImage($sheet)
$g.SmoothingMode = 'AntiAlias'
$g.InterpolationMode = 'HighQualityBicubic'
$g.Clear([Drawing.Color]::FromArgb(255, 10, 13, 17))
$light = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(255, 150, 160, 170))
$g.FillRectangle($light, 0, ($cell * 2), ($cell * 5), ($cell * 2))
$rows = @(@('GP_LeftStick', 'LeftStick'), @('GP_RightStick', 'RightStick'))
for ($band = 0; $band -lt 2; $band++)
{
    for ($r = 0; $r -lt 2; $r++)
    {
        $files = @("$($rows[$r][0]).png") + (@('Up', 'Down', 'Left', 'Right') | ForEach-Object { "GP_$($rows[$r][1])_$_.png" })
        for ($i = 0; $i -lt $files.Count; $i++)
        {
            $img = [Drawing.Image]::FromFile((Join-Path $OutDir $files[$i]))
            $g.DrawImage($img, ($i * $cell + 9), (($band * 2 + $r) * $cell + 9), 112, 112)
            $img.Dispose()
        }
    }
}
$g.Dispose()
$sheet.Save((Join-Path $OutDir 'StickDirections_Preview.png'), [Drawing.Imaging.ImageFormat]::Png)
$sheet.Dispose()

$names
