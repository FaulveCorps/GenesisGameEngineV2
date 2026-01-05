# Render a visual diff between two ScreenToGif frame PNGs.
# Produces a 3-panel image: A (left), B (middle), abs-diff heatmap (right), cropped to the changed region.

param(
  [Parameter(Mandatory = $false)]
  [string]$FramesDir = "C:\Users\jpfau\AppData\Local\Temp\ScreenToGif\Recording\2026-01-05 21-05-16\Clipboard\1",

  [Parameter(Mandatory = $true)]
  [int]$A,

  [Parameter(Mandatory = $true)]
  [int]$B,

  [Parameter(Mandatory = $false)]
  [string]$OutPath = "C:\Users\jpfau\Desktop\Project\GenesisGameEngine\artifacts\diff.png",

  [Parameter(Mandatory = $false)]
  [int]$Threshold = 45,

  [Parameter(Mandatory = $false)]
  [int]$Pad = 24,

  [Parameter(Mandatory = $false)]
  [int]$Scale = 2
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $FramesDir)) {
  throw "FramesDir not found: $FramesDir"
}

$aPath = Join-Path $FramesDir ("{0}.png" -f $A)
$bPath = Join-Path $FramesDir ("{0}.png" -f $B)

if (-not (Test-Path -LiteralPath $aPath)) { throw "Missing frame: $aPath" }
if (-not (Test-Path -LiteralPath $bPath)) { throw "Missing frame: $bPath" }

$null = New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutPath)

Add-Type -AssemblyName System.Drawing

function Clamp {
  param(
    [Parameter(Mandatory = $true)][int]$v,
    [Parameter(Mandatory = $true)][int]$lo,
    [Parameter(Mandatory = $true)][int]$hi
  )
  if ($v -lt $lo) { return $lo }
  if ($v -gt $hi) { return $hi }
  return $v
}

function ResizeBitmap([System.Drawing.Bitmap]$src, [int]$w, [int]$h) {
  $dst = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $g = [System.Drawing.Graphics]::FromImage($dst)
  try {
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighSpeed
    $g.DrawImage($src, 0, 0, $w, $h)
  }
  finally {
    $g.Dispose()
  }
  return $dst
}

function CropBitmap([System.Drawing.Bitmap]$src, [int]$x, [int]$y, [int]$w, [int]$h) {
  $rect = New-Object System.Drawing.Rectangle($x, $y, $w, $h)
  return $src.Clone($rect, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
}

function ComputeDiffAndBBox([System.Drawing.Bitmap]$aBmp, [System.Drawing.Bitmap]$bBmp, [int]$threshold) {
  if ($aBmp.Width -ne $bBmp.Width -or $aBmp.Height -ne $bBmp.Height) {
    throw "Frame sizes differ: A=$($aBmp.Width)x$($aBmp.Height) B=$($bBmp.Width)x$($bBmp.Height)"
  }

  $w = $aBmp.Width
  $h = $aBmp.Height
  $rect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)

  $dataA = $aBmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $dataB = $bBmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)

  $diffBmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $dataD = $diffBmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)

  try {
    $stride = [Math]::Abs($dataA.Stride)
    $bytes = $stride * $h

    $bufA = New-Object byte[] $bytes
    $bufB = New-Object byte[] $bytes
    $bufD = New-Object byte[] $bytes

    [System.Runtime.InteropServices.Marshal]::Copy($dataA.Scan0, $bufA, 0, $bytes)
    [System.Runtime.InteropServices.Marshal]::Copy($dataB.Scan0, $bufB, 0, $bytes)

    $minX = $w
    $minY = $h
    $maxX = -1
    $maxY = -1

    for ($y = 0; $y -lt $h; $y++) {
      $row = $y * $stride
      for ($x = 0; $x -lt $w; $x++) {
        $i = $row + ($x * 3)

        $db = [Math]::Abs([int]$bufA[$i] - [int]$bufB[$i])
        $dg = [Math]::Abs([int]$bufA[$i + 1] - [int]$bufB[$i + 1])
        $dr = [Math]::Abs([int]$bufA[$i + 2] - [int]$bufB[$i + 2])
        $d = $db + $dg + $dr

        # Heatmap: map diff magnitude to red channel, keep some brightness in G/B for visibility.
        $r = [byte](Clamp -v ([int]($d / 3)) -lo 0 -hi 255)
        $g = [byte](Clamp -v ([int]($d / 10)) -lo 0 -hi 255)
        $b = [byte](Clamp -v ([int]($d / 10)) -lo 0 -hi 255)

        $bufD[$i]     = $b
        $bufD[$i + 1] = $g
        $bufD[$i + 2] = $r

        if ($d -ge $threshold) {
          if ($x -lt $minX) { $minX = $x }
          if ($y -lt $minY) { $minY = $y }
          if ($x -gt $maxX) { $maxX = $x }
          if ($y -gt $maxY) { $maxY = $y }
        }
      }
    }

    [System.Runtime.InteropServices.Marshal]::Copy($bufD, 0, $dataD.Scan0, $bytes)

    return [pscustomobject]@{
      Diff = $diffBmp
      MinX = $(if ($maxX -ge 0) { $minX } else { $null })
      MinY = $(if ($maxX -ge 0) { $minY } else { $null })
      MaxX = $(if ($maxX -ge 0) { $maxX } else { $null })
      MaxY = $(if ($maxX -ge 0) { $maxY } else { $null })
    }
  }
  finally {
    $aBmp.UnlockBits($dataA)
    $bBmp.UnlockBits($dataB)
    $diffBmp.UnlockBits($dataD)
  }
}

$aBmp = [System.Drawing.Bitmap]::FromFile($aPath)
$bBmp = [System.Drawing.Bitmap]::FromFile($bPath)

try {
  $res = ComputeDiffAndBBox -aBmp $aBmp -bBmp $bBmp -threshold $Threshold
  $diffBmp = $res.Diff
  try {
    $w = $aBmp.Width
    $h = $aBmp.Height

    if ($null -eq $res.MinX) {
      # No significant changes detected: use full frame.
      $x0 = 0; $y0 = 0; $cw = $w; $ch = $h
    }
    else {
      $x0 = Clamp -v ($res.MinX - $Pad) -lo 0 -hi ($w - 1)
      $y0 = Clamp -v ($res.MinY - $Pad) -lo 0 -hi ($h - 1)
      $x1 = Clamp -v ($res.MaxX + $Pad) -lo 0 -hi ($w - 1)
      $y1 = Clamp -v ($res.MaxY + $Pad) -lo 0 -hi ($h - 1)
      $cw = ($x1 - $x0 + 1)
      $ch = ($y1 - $y0 + 1)
    }

    $aCrop = CropBitmap -src $aBmp -x $x0 -y $y0 -w $cw -h $ch
    $bCrop = CropBitmap -src $bBmp -x $x0 -y $y0 -w $cw -h $ch
    $dCrop = CropBitmap -src $diffBmp -x $x0 -y $y0 -w $cw -h $ch

    try {
      $sw = $cw * $Scale
      $sh = $ch * $Scale

      $aScaled = ResizeBitmap -src $aCrop -w $sw -h $sh
      $bScaled = ResizeBitmap -src $bCrop -w $sw -h $sh
      $dScaled = ResizeBitmap -src $dCrop -w $sw -h $sh

      try {
        $padding = 12
        $labelH = 28
        $outW = ($sw * 3) + ($padding * 4)
        $outH = $sh + ($padding * 3) + $labelH

        $outBmp = New-Object System.Drawing.Bitmap($outW, $outH, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        $g = [System.Drawing.Graphics]::FromImage($outBmp)
        try {
          $g.Clear([System.Drawing.Color]::FromArgb(30, 30, 30))
          $font = New-Object System.Drawing.Font("Segoe UI", 12, [System.Drawing.FontStyle]::Bold)
          $brush = [System.Drawing.Brushes]::White

          $yImg = $padding + $labelH
          $xA = $padding
          $xB = $padding * 2 + $sw
          $xD = $padding * 3 + $sw * 2

          $g.DrawString(("A: {0}.png" -f $A), $font, $brush, $xA, $padding)
          $g.DrawString(("B: {0}.png" -f $B), $font, $brush, $xB, $padding)
          $g.DrawString(("Diff (thr={0})" -f $Threshold), $font, $brush, $xD, $padding)

          # Helpful provenance for mapping diffs back to the original frame.
          $metaLine = "crop=({0},{1},{2},{3}) scale={4}" -f $x0, $y0, $cw, $ch, $Scale
          $g.DrawString($metaLine, $font, $brush, $xD, ($padding + 14))

          $g.DrawImage($aScaled, $xA, $yImg, $sw, $sh)
          $g.DrawImage($bScaled, $xB, $yImg, $sw, $sh)
          $g.DrawImage($dScaled, $xD, $yImg, $sw, $sh)

          $outBmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
          $g.Dispose()
          $font.Dispose()
          $outBmp.Dispose()
        }

        # Write a JSON sidecar with enough metadata to map panel/hotspot coordinates
        # back to original frame coordinates (for automation scripts).
        $metaPath = [System.IO.Path]::ChangeExtension($OutPath, ".json")
        $meta = [pscustomobject]@{
          a = $A
          b = $B
          framesDir = $FramesDir
          threshold = $Threshold
          pad = $Pad
          scale = $Scale
          source = @{ w = $w; h = $h }
          crop = @{ x = $x0; y = $y0; w = $cw; h = $ch }
          scaled = @{ w = $sw; h = $sh }
          composite = @{ w = $outW; h = $outH; padding = $padding; labelH = $labelH }
          panels = @{
            a = @{ x = $xA; y = $yImg; w = $sw; h = $sh }
            b = @{ x = $xB; y = $yImg; w = $sw; h = $sh }
            diff = @{ x = $xD; y = $yImg; w = $sw; h = $sh }
          }
        }

        $meta | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $metaPath -Encoding UTF8
      }
      finally {
        $aScaled.Dispose(); $bScaled.Dispose(); $dScaled.Dispose()
      }
    }
    finally {
      $aCrop.Dispose(); $bCrop.Dispose(); $dCrop.Dispose()
    }
  }
  finally {
    $diffBmp.Dispose()
  }
}
finally {
  $aBmp.Dispose()
  $bBmp.Dispose()
}

Write-Host "Wrote: $OutPath"
