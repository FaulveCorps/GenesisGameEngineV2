<#
Annotate source frames with hotspot bounding boxes derived from the diff analysis.

Workflow:
1) Ensure diffs were generated with Scripts/render_frame_diff.ps1 (this creates a .json sidecar).
2) Run Scripts/analyze_diff_bbox.ps1 to locate the hottest 32x32 block in the diff panel.
3) This script re-runs analyze_diff_bbox.ps1, reads each diff's meta .json, and draws the
   hotspot box on the *source* frame (frame B by default).

Output:
  artifacts/hot_src_<diffName>_B.png

Notes:
  - Requires System.Drawing (Windows).
  - Coordinates are mapped back to the source frame using meta.scale and meta.crop.
#>

param(
  [Parameter(Mandatory=$false)]
  [string]$ArtifactsDir = (Join-Path $PSScriptRoot '..' 'artifacts'),

  [Parameter(Mandatory=$false)]
  [int]$Threshold = 80,

  [Parameter(Mandatory=$false)]
  [ValidateSet('A','B')]
  [string]$AnnotateFrame = 'B',

  [Parameter(Mandatory=$false)]
  [int]$OutScale = 3
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

function Parse-BBox {
  param([Parameter(Mandatory=$true)][string]$BBox)
  $p = $BBox -split ','
  if ($p.Length -ne 4) { return $null }
  return [pscustomobject]@{ x0=[int]$p[0]; y0=[int]$p[1]; x1=[int]$p[2]; y1=[int]$p[3] }
}

function ResizeBitmapNearest {
  param(
    [Parameter(Mandatory=$true)][System.Drawing.Bitmap]$Source,
    [Parameter(Mandatory=$true)][int]$Scale
  )

  # Defensive: when functions are invoked with unexpected argument binding,
  # PowerShell can sometimes feed arrays here. Coerce to the first element.
  if ($Source -is [System.Array]) {
    $Source = [System.Drawing.Bitmap]$Source[0]
  }
  if ($Scale -is [System.Array]) {
    $Scale = [int]$Scale[0]
  }

  if ($Scale -le 1) { return $Source.Clone() }

  $dst = New-Object System.Drawing.Bitmap(([int]$Source.Width * $Scale), ([int]$Source.Height * $Scale), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $g = [System.Drawing.Graphics]::FromImage($dst)
  try {
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighSpeed
    $g.DrawImage($Source, 0, 0, $dst.Width, $dst.Height)
  }
  finally {
    $g.Dispose()
  }
  return $dst
}

$ArtifactsDir = (Resolve-Path $ArtifactsDir).Path

# Capture analysis output (CSV) as objects.
$analysisScript = Join-Path $PSScriptRoot 'analyze_diff_bbox.ps1'
if (-not (Test-Path -LiteralPath $analysisScript)) {
  throw "Missing analysis script: $analysisScript"
}

$csvText = & $analysisScript -ArtifactsDir $ArtifactsDir -Threshold $Threshold
$rows = $csvText | ConvertFrom-Csv

foreach ($row in $rows) {
  if (-not $row.file) { continue }

  $diffPng = Join-Path $ArtifactsDir $row.file
  $metaPath = [System.IO.Path]::ChangeExtension($diffPng, '.json')
  if (-not (Test-Path -LiteralPath $metaPath)) {
    Write-Warning "Skipping (no meta json): $($row.file)"
    continue
  }

  if (-not $row.hotBBoxSrc) {
    Write-Warning "Skipping (no hotBBoxSrc): $($row.file)"
    continue
  }

  $meta = Get-Content -LiteralPath $metaPath -Raw | ConvertFrom-Json
  $frameIndex = if ($AnnotateFrame -eq 'A') { [int]$meta.a } else { [int]$meta.b }
  $framePath = Join-Path $meta.framesDir ("{0}.png" -f $frameIndex)
  if (-not (Test-Path -LiteralPath $framePath)) {
    Write-Warning "Skipping (missing frame file): $framePath"
    continue
  }

  $hb = Parse-BBox -BBox $row.hotBBoxSrc
  if ($null -eq $hb) {
    Write-Warning "Skipping (invalid hotBBoxSrc): $($row.file) -> $($row.hotBBoxSrc)"
    continue
  }

  $src = [System.Drawing.Bitmap]::FromFile($framePath)
  try {
    # Draw in original resolution first.
    $g = [System.Drawing.Graphics]::FromImage($src)
    try {
      $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::Lime, 2)
      $pen2 = New-Object System.Drawing.Pen([System.Drawing.Color]::Black, 4)
      $font = New-Object System.Drawing.Font('Consolas', 10, [System.Drawing.FontStyle]::Bold)
      $brush = [System.Drawing.Brushes]::White
      $shadow = [System.Drawing.Brushes]::Black

      $rw = ($hb.x1 - $hb.x0 + 1)
      $rh = ($hb.y1 - $hb.y0 + 1)
      $rect = New-Object System.Drawing.Rectangle($hb.x0, $hb.y0, $rw, $rh)

      # Outline with a black shadow for contrast, then lime.
      $g.DrawRectangle($pen2, $rect)
      $g.DrawRectangle($pen, $rect)

      $label = "hot: {0} | frame {1} ({2})" -f $row.hotBBoxSrc, $frameIndex, $AnnotateFrame
      $g.DrawString($label, $font, $shadow, 6, 6)
      $g.DrawString($label, $font, $brush, 5, 5)
    }
    finally {
      $g.Dispose()
      if ($pen) { $pen.Dispose() }
      if ($pen2) { $pen2.Dispose() }
      if ($font) { $font.Dispose() }
    }

    $outBase = [System.IO.Path]::GetFileNameWithoutExtension($row.file)
    $outPath = Join-Path $ArtifactsDir ("hot_src_{0}_{1}.png" -f $outBase, $AnnotateFrame)

    $scaled = ResizeBitmapNearest -Source $src -Scale $OutScale
    try {
      $scaled.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
      Write-Host "Wrote: $outPath"
    }
    finally {
      $scaled.Dispose()
    }
  }
  finally {
    $src.Dispose()
  }
}
