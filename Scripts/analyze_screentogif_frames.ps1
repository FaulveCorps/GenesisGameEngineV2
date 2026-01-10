# Analyze a ScreenToGif PNG frame folder by computing simple per-frame pixel deltas.
# Output: artifacts/frame_deltas.csv

param(
  [Parameter(Mandatory = $false)]
  [string]$FramesDir = "C:\Users\jpfau\AppData\Local\Temp\ScreenToGif\Recording\2026-01-05 21-05-16\Clipboard\1",

  [Parameter(Mandatory = $false)]
  [string]$OutCsv = "C:\Users\jpfau\Desktop\Project\GenesisGameEngine\artifacts\frame_deltas.csv",

  [Parameter(Mandatory = $false)]
  [int]$TargetWidth = 160,

  [Parameter(Mandatory = $false)]
  [int]$TargetHeight = 90,

  [Parameter(Mandatory = $false)]
  [int]$TopN = 15

  ,
  [Parameter(Mandatory = $false)]
  [int]$ChangeThreshold = 45
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $FramesDir)) {
  throw "FramesDir not found: $FramesDir"
}

$null = New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutCsv)

Add-Type -AssemblyName System.Drawing

function Get-DownsampledBitmap([string]$Path, [int]$W, [int]$H) {
  $src = [System.Drawing.Bitmap]::FromFile($Path)
  try {
    $dst = New-Object System.Drawing.Bitmap($W, $H, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($dst)
    try {
      $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBilinear
      $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
      $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighSpeed
      $g.DrawImage($src, 0, 0, $W, $H)
    }
    finally {
      $g.Dispose()
    }
    return $dst
  }
  finally {
    $src.Dispose()
  }
}

function Get-DeltaStats([System.Drawing.Bitmap]$A, [System.Drawing.Bitmap]$B, [int]$Threshold) {
  if ($A.Width -ne $B.Width -or $A.Height -ne $B.Height) {
    throw "Bitmap sizes differ"
  }

  $w = $A.Width
  $h = $A.Height

  $rect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
  $dataA = $A.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $dataB = $B.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  try {
    $stride = [Math]::Abs($dataA.Stride)
    $bytes = $stride * $h

    $bufA = New-Object byte[] $bytes
    $bufB = New-Object byte[] $bytes

    [System.Runtime.InteropServices.Marshal]::Copy($dataA.Scan0, $bufA, 0, $bytes)
    [System.Runtime.InteropServices.Marshal]::Copy($dataB.Scan0, $bufB, 0, $bytes)

    $sumAll = 0L
    $sumTR = 0L; $sumTL = 0L; $sumBR = 0L; $sumBL = 0L

    $changed = 0L
    $minX = $w
    $minY = $h
    $maxX = -1
    $maxY = -1

    for ($y = 0; $y -lt $h; $y++) {
      $row = $y * $stride
      $isTop = $y -lt [int]($h / 2)

      for ($x = 0; $x -lt $w; $x++) {
        $i = $row + ($x * 3)

        $db = [Math]::Abs([int]$bufA[$i] - [int]$bufB[$i])
        $dg = [Math]::Abs([int]$bufA[$i + 1] - [int]$bufB[$i + 1])
        $dr = [Math]::Abs([int]$bufA[$i + 2] - [int]$bufB[$i + 2])
        $d = $db + $dg + $dr

        $sumAll += $d

        $isRight = $x -ge [int]($w / 2)
        if ($isTop) {
          if ($isRight) { $sumTR += $d } else { $sumTL += $d }
        }
        else {
          if ($isRight) { $sumBR += $d } else { $sumBL += $d }
        }

        if ($d -ge $Threshold) {
          $changed += 1
          if ($x -lt $minX) { $minX = $x }
          if ($y -lt $minY) { $minY = $y }
          if ($x -gt $maxX) { $maxX = $x }
          if ($y -gt $maxY) { $maxY = $y }
        }
      }
    }

    return [pscustomobject]@{
      SumAll = $sumAll
      SumTR = $sumTR
      SumTL = $sumTL
      SumBR = $sumBR
      SumBL = $sumBL
      Width = $w
      Height = $h
      ChangedPixels = $changed
      BBoxMinX = $(if ($maxX -ge 0) { $minX } else { $null })
      BBoxMinY = $(if ($maxX -ge 0) { $minY } else { $null })
      BBoxMaxX = $(if ($maxX -ge 0) { $maxX } else { $null })
      BBoxMaxY = $(if ($maxX -ge 0) { $maxY } else { $null })
    }
  }
  finally {
    $A.UnlockBits($dataA)
    $B.UnlockBits($dataB)
  }
}

$files = Get-ChildItem -LiteralPath $FramesDir -Filter "*.png" | Sort-Object { [int]($_.BaseName) }
if ($files.Count -lt 2) {
  throw "Not enough frames in $FramesDir (need >= 2, got $($files.Count))"
}

Write-Host "Dir:    $FramesDir"
Write-Host "Frames: $($files.Count)"
Write-Host "First:  $($files[0].Name)"
Write-Host "Last:   $($files[$files.Count - 1].Name)"

$deltas = New-Object System.Collections.Generic.List[object]

$prevBmp = $null
$prevName = $null

for ($idx = 0; $idx -lt $files.Count; $idx++) {
  $path = $files[$idx].FullName
  $name = $files[$idx].Name

  $bmp = Get-DownsampledBitmap -Path $path -W $TargetWidth -H $TargetHeight

  if ($null -ne $prevBmp) {
    $stats = Get-DeltaStats -A $prevBmp -B $bmp -Threshold $ChangeThreshold
    $pixels = $stats.Width * $stats.Height

    $bboxW = $(if ($null -ne $stats.BBoxMinX) { ($stats.BBoxMaxX - $stats.BBoxMinX + 1) } else { 0 })
    $bboxH = $(if ($null -ne $stats.BBoxMinY) { ($stats.BBoxMaxY - $stats.BBoxMinY + 1) } else { 0 })
    $changedPct = $(if ($pixels -gt 0) { [double]$stats.ChangedPixels / $pixels } else { 0.0 })

    $deltas.Add([pscustomobject]@{
      Prev = $prevName
      Curr = $name
      SumAll = $stats.SumAll
      AvgAll = [double]$stats.SumAll / $pixels
      AvgTR = [double]$stats.SumTR / ($pixels / 4)
      AvgTL = [double]$stats.SumTL / ($pixels / 4)
      AvgBR = [double]$stats.SumBR / ($pixels / 4)
      AvgBL = [double]$stats.SumBL / ($pixels / 4)
      ChangedPct = $changedPct
      BBoxMinX = $stats.BBoxMinX
      BBoxMinY = $stats.BBoxMinY
      BBoxMaxX = $stats.BBoxMaxX
      BBoxMaxY = $stats.BBoxMaxY
      BBoxW = $bboxW
      BBoxH = $bboxH
    }) | Out-Null
  }

  if ($null -ne $prevBmp) { $prevBmp.Dispose() }
  $prevBmp = $bmp
  $prevName = $name
}

if ($null -ne $prevBmp) { $prevBmp.Dispose() }

$deltas | Export-Csv -NoTypeInformation -Encoding UTF8 -Path $OutCsv

Write-Host "Wrote:  $OutCsv"
Write-Host ""
Write-Host "Top $TopN by AvgAll:" 
$deltas | Sort-Object AvgAll -Descending | Select-Object -First $TopN | Format-Table Prev, Curr, AvgAll, ChangedPct, BBoxW, BBoxH, AvgTR, AvgTL, AvgBR, AvgBL -AutoSize
Write-Host ""
Write-Host "Top 10 by AvgTR (top-right quadrant):" 
$deltas | Sort-Object AvgTR -Descending | Select-Object -First 10 | Format-Table Prev, Curr, AvgTR, AvgAll, ChangedPct, BBoxW, BBoxH -AutoSize
