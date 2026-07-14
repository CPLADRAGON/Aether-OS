Add-Type -AssemblyName System.Drawing.Common

$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'oled-ui-gallery.png'

# Documentation renderer, not a hardware capture:
# - draws each current screen into a 64x48 1-bit-like raster first
# - uses only opaque white/black pixels and nearest-neighbor scaling
# - follows current firmware positions and menu order
# The host lacks a runnable U8g2 C renderer, so Lucida Console is used for
# text. Layout coordinates and content are sourced from current main.cpp.
$scale = 4
$fontSmall = New-Object System.Drawing.Font('Lucida Console', 5.8, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$fontNormal = New-Object System.Drawing.Font('Lucida Console', 8.5, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$fontLarge = New-Object System.Drawing.Font('Lucida Console', 16, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$white = [System.Drawing.Brushes]::White
$black = [System.Drawing.Brushes]::Black
$pen = New-Object System.Drawing.Pen([System.Drawing.Color]::White, 1)

function New-Screen {
  $bmp = New-Object System.Drawing.Bitmap(64,48)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.Clear([System.Drawing.Color]::Black)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
  $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
  return @($bmp,$g)
}

function Txt($g,$text,$x,$y,$font=$fontSmall,$invert=$false) {
  if ($invert) { $g.DrawString($text,$font,$black,$x,$y) } else { $g.DrawString($text,$font,$white,$x,$y) }
}
function Header($g,$title) {
  $g.FillRectangle($white,0,0,64,10)
  Txt $g $title 2 1 $fontSmall $true
}
function Box($g,$x,$y,$w,$h) { $g.DrawRectangle($pen,$x,$y,$w-1,$h-1) }
function Sun($g,$cx,$cy,$r=5) {
  $g.DrawEllipse($pen,$cx-$r,$cy-$r,$r*2,$r*2)
  foreach ($p in @(@(0,-8,0,-5),@(0,5,0,8),@(-8,0,-5,0),@(5,0,8,0),@(-6,-6,-4,-4),@(4,4,6,6),@(-6,6,-4,4),@(4,-4,6,-6))) {
    $g.DrawLine($pen,$cx+$p[0],$cy+$p[1],$cx+$p[2],$cy+$p[3])
  }
}
function Screen($name,$draw) {
  $pair = New-Screen; $bmp=$pair[0]; $g=$pair[1]
  & $draw $g
  $g.Dispose()
  return @{ Name=$name; Bmp=$bmp }
}

$screens = @()
$screens += Screen 'MENU' {
  param($g); Header $g 'MENU'; $g.FillRectangle($white,9,13,46,8); Txt $g 'MEASURE' 14 14 $fontSmall $true; Txt $g 'TIME' 19 24; Txt $g 'WEATHER' 14 31; Txt $g 'ROOM' 19 38
}
$screens += Screen 'MEASURE' {
  param($g); Header $g 'SCAN 3/5'; Txt $g '29.4C' 26 14 $fontNormal; Txt $g '68%' 27 26; $g.FillRectangle($white,0,39,64,9); Txt $g '420 BRIGHT' 7 40 $fontSmall $true
}
$screens += Screen 'TIME' {
  param($g); Txt $g '14:32' 7 10 $fontLarge; $g.DrawLine($pen,2,37,62,37); Txt $g 'THU' 2 39; Txt $g '10 JUL' 39 39
}
$screens += Screen 'TIME DETAIL' {
  param($g); Header $g '14:32:47'; Txt $g 'THU' 50 1 $fontSmall $true; Txt $g '10 JUL 2026' 2 13; Txt $g 'WK28 D191' 2 22; $g.DrawLine($pen,2,31,62,31); Txt $g 'TODAY' 2 34; Txt $g '61%' 48 34; Box $g 2 43 60 3; $g.FillRectangle($white,3,44,35,1)
}
$screens += Screen 'WEATHER' {
  param($g); $g.FillRectangle($white,0,0,64,10); Sun $g 7 5 3; Txt $g '82% HUM' 37 1 $fontSmall $true; Txt $g '31.5C' 7 15 $fontLarge; Txt $g 'FL 37.0C' 2 39; Txt $g '>' 58 39
}
$screens += Screen 'WEATHER DETAIL' {
  param($g); Sun $g 32 13 8; Txt $g 'SUNNY' 19 27; $g.DrawLine($pen,2,36,62,36); Txt $g '31.5C 82%' 14 39
}
$screens += Screen 'ROOM' {
  param($g); Txt $g '29.4C' 7 14 $fontLarge; $g.DrawLine($pen,2,37,62,37); Txt $g '68%' 2 39; Txt $g 'BRIGHT>' 35 39
}
$screens += Screen 'ROOM STATUS' {
  param($g); Header $g 'STATUS'; Txt $g 'T:WARM 29.4C' 2 16; Txt $g 'H:HUMID 68%' 2 26; Txt $g 'L:BRIGHT 420' 2 36
}
$screens += Screen 'TIMER' {
  param($g); Header $g 'TIMER'; Txt $g '14:28' 11 12 $fontLarge; Box $g 2 34 60 4; $g.FillRectangle($white,3,35,28,2); Txt $g 'HOLD:END' 25 40
}
$screens += Screen 'ALERT' {
  param($g); Header $g 'TIMER'; Txt $g 'DONE!' 16 16 $fontNormal; Txt $g 'ANY KEY TO DISMISS' 2 40
}

$cols=5; $cellW=256; $cellH=192; $labelH=24; $pad=28; $rows=2
$canvas = New-Object System.Drawing.Bitmap(1448, 500)
$cg=[System.Drawing.Graphics]::FromImage($canvas)
$cg.Clear([System.Drawing.Color]::FromArgb(13,13,15))
$cg.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$cg.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
$labelFont=New-Object System.Drawing.Font('Arial',10,[System.Drawing.FontStyle]::Bold)
for($i=0;$i -lt $screens.Count;$i++){
  $r=[math]::Floor($i/$cols);$c=$i%$cols
  $x=$pad+$c*($cellW+$pad);$y=$pad+$r*($cellH+$labelH+$pad)
  $dst=New-Object System.Drawing.Rectangle($x,$y,$cellW,$cellH)
  $cg.DrawImage($screens[$i].Bmp,$dst)
  $cg.DrawString($screens[$i].Name,$labelFont,[System.Drawing.Brushes]::LightGray,$x,$y+$cellH+5)
  $screens[$i].Bmp.Dispose()
}
$canvas.Save($out,[System.Drawing.Imaging.ImageFormat]::Png)
$cg.Dispose();$canvas.Dispose()
$fontSmall.Dispose();$fontNormal.Dispose();$fontLarge.Dispose();$labelFont.Dispose();$pen.Dispose()
Write-Output $out
