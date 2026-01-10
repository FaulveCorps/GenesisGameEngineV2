param(
  [Parameter(Mandatory=$false)]
  [string]$ArtifactsDir = (Join-Path $PSScriptRoot '..' 'artifacts'),

  [Parameter(Mandatory=$false)]
  [int]$Threshold = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

function Try-LoadDiffMeta {
  param(
    [Parameter(Mandatory=$true)][string]$ImagePath
  )

  $metaPath = [System.IO.Path]::ChangeExtension($ImagePath, '.json')
  if (-not (Test-Path -LiteralPath $metaPath)) {
    return $null
  }

  try {
    return (Get-Content -LiteralPath $metaPath -Raw | ConvertFrom-Json)
  }
  catch {
    Write-Warning "Failed to parse meta JSON: $metaPath ($($_.Exception.Message))"
    return $null
  }
}

function Get-DiffPanelRegion {
  param(
    [Parameter(Mandatory=$true)][string]$ImagePath,
    [Parameter(Mandatory=$true)][int]$ImageWidth,
    [Parameter(Mandatory=$true)][int]$ImageHeight
  )

  $meta = Try-LoadDiffMeta -ImagePath $ImagePath
  if ($null -ne $meta -and $null -ne $meta.panels -and $null -ne $meta.panels.diff) {
    $x0 = [int]$meta.panels.diff.x
    $y0 = [int]$meta.panels.diff.y
    $pw = [int]$meta.panels.diff.w
    $ph = [int]$meta.panels.diff.h

    $x1 = $x0 + $pw - 1
    $y1 = $y0 + $ph - 1

    # Clamp to image bounds for robustness.
    $x0 = [math]::Max(0, [math]::Min($ImageWidth - 1, $x0))
    $y0 = [math]::Max(0, [math]::Min($ImageHeight - 1, $y0))
    $x1 = [math]::Max(0, [math]::Min($ImageWidth - 1, $x1))
    $y1 = [math]::Max(0, [math]::Min($ImageHeight - 1, $y1))

    return [pscustomobject]@{
      hasMeta = $true
      meta = $meta
      x0 = $x0
      y0 = $y0
      x1 = $x1
      y1 = $y1
      panelW = ($x1 - $x0 + 1)
      panelH = ($y1 - $y0 + 1)
    }
  }

  # Fallback: 3 equal-width panels (no metadata available)
  $panels = 1
  $panelW = $ImageWidth
  if (($ImageWidth % 3) -eq 0) {
    $panels = 3
    $panelW = [int]($ImageWidth / 3)
  }
  $diffPanelIndex = if ($panels -eq 3) { 2 } else { 0 }
  $x0 = $diffPanelIndex * $panelW
  $x1 = $x0 + $panelW - 1

  return [pscustomobject]@{
    hasMeta = $false
    meta = $null
    x0 = $x0
    y0 = 0
    x1 = $x1
    y1 = ($ImageHeight - 1)
    panelW = $panelW
    panelH = $ImageHeight
  }
}

function Parse-BBox {
  param(
    [Parameter(Mandatory=$true)][string]$BBox
  )

  $p = $BBox -split ','
  if ($p.Length -ne 4) { return $null }
  return [pscustomobject]@{
    x0 = [int]$p[0]
    y0 = [int]$p[1]
    x1 = [int]$p[2]
    y1 = [int]$p[3]
  }
}

function Get-NonBlackBBox {
  param(
    [Parameter(Mandatory=$true)][string]$Path,
    [Parameter(Mandatory=$true)][int]$Thresh
  )

  $bmp = [System.Drawing.Bitmap]::FromFile($Path)
  try {
    $w = $bmp.Width
    $h = $bmp.Height

    $region = Get-DiffPanelRegion -ImagePath $Path -ImageWidth $w -ImageHeight $h

    $rect = New-Object System.Drawing.Rectangle 0,0,$w,$h
    $data = $bmp.LockBits(
      $rect,
      [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
      [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    )

    try {
      $stride = $data.Stride
      $bytes = New-Object byte[] ($stride * $h)
      [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)

      $minX = $w
      $minY = $h
      $maxX = -1
      $maxY = -1

      [long]$sumX = 0
      [long]$sumY = 0
      [long]$count = 0

      function Scan-Region {
        param(
          [Parameter(Mandatory=$true)][int]$X0,
          [Parameter(Mandatory=$true)][int]$X1,
          [Parameter(Mandatory=$true)][int]$Y0,
          [Parameter(Mandatory=$true)][int]$Y1
        )

        $minXr = $X1
        $minYr = $Y1
        $maxXr = -1
        $maxYr = -1
        [long]$sumXr = 0
        [long]$sumYr = 0
        [long]$countr = 0

        for ($yy = $Y0; $yy -le $Y1; $yy++) {
          $row = $yy * $stride
          for ($xx = $X0; $xx -le $X1; $xx++) {
            $i = $row + ($xx * 4)
            $b = $bytes[$i]
            $g = $bytes[$i + 1]
            $r = $bytes[$i + 2]
            $a = $bytes[$i + 3]

            if ($a -gt 0 -and (($r -gt $Thresh) -or ($g -gt $Thresh) -or ($b -gt $Thresh))) {
              if ($xx -lt $minXr) { $minXr = $xx }
              if ($yy -lt $minYr) { $minYr = $yy }
              if ($xx -gt $maxXr) { $maxXr = $xx }
              if ($yy -gt $maxYr) { $maxYr = $yy }

              $sumXr += $xx
              $sumYr += $yy
              $countr++
            }
          }
        }

        if ($countr -eq 0) {
          return [pscustomobject]@{ count = 0; pct = 0; bbox = $null; cx = $null; cy = $null }
        }

        $cxr = [double]$sumXr / $countr
        $cyr = [double]$sumYr / $countr
        $area = ($X1 - $X0 + 1) * ($Y1 - $Y0 + 1)
        $pctr = 100.0 * $countr / $area

        return [pscustomobject]@{
          count = $countr
          pct = [math]::Round($pctr, 3)
          bbox = "$minXr,$minYr,$maxXr,$maxYr"
          cx = [math]::Round($cxr, 1)
          cy = [math]::Round($cyr, 1)
        }
      }

      function Find-HotBlock {
        param(
          [Parameter(Mandatory=$true)][int]$X0,
          [Parameter(Mandatory=$true)][int]$X1,
          [Parameter(Mandatory=$true)][int]$Y0,
          [Parameter(Mandatory=$true)][int]$Y1,
          [Parameter(Mandatory=$true)][int]$BlockSize
        )

        $width = $X1 - $X0 + 1
        $height = $Y1 - $Y0 + 1
        $bw = [int][math]::Ceiling($width / $BlockSize)
        $bh = [int][math]::Ceiling($height / $BlockSize)
        $counts = New-Object int[] ($bw * $bh)

        for ($yy = $Y0; $yy -le $Y1; $yy++) {
          $row = $yy * $stride
          # IMPORTANT: PowerShell's [int] cast rounds; we need floor for stable bucketization.
          $by = [int][math]::Floor((($yy - $Y0) / [double]$BlockSize))
          for ($xx = $X0; $xx -le $X1; $xx++) {
            $i = $row + ($xx * 4)
            $b = $bytes[$i]
            $g = $bytes[$i + 1]
            $r = $bytes[$i + 2]
            $a = $bytes[$i + 3]
            if ($a -gt 0 -and (($r -gt $Thresh) -or ($g -gt $Thresh) -or ($b -gt $Thresh))) {
              $bx = [int][math]::Floor((($xx - $X0) / [double]$BlockSize))
              $idx = ($by * $bw) + $bx
              $counts[$idx]++
            }
          }
        }

        $max = -1
        $maxIdx = -1
        for ($i = 0; $i -lt $counts.Length; $i++) {
          if ($counts[$i] -gt $max) {
            $max = $counts[$i]
            $maxIdx = $i
          }
        }

        if ($max -le 0) {
          return [pscustomobject]@{ hotCount = 0; hotPct = 0; hotBBoxRel = $null }
        }

        $maxBy = [int][math]::Floor($maxIdx / $bw)
        $maxBx = $maxIdx - ($maxBy * $bw)

        $hx0 = $X0 + ($maxBx * $BlockSize)
        $hy0 = $Y0 + ($maxBy * $BlockSize)
        $hx1 = [math]::Min($X1, $hx0 + $BlockSize - 1)
        $hy1 = [math]::Min($Y1, $hy0 + $BlockSize - 1)
        $area = ($hx1 - $hx0 + 1) * ($hy1 - $hy0 + 1)
        $hpct = 100.0 * $max / $area

        # Return bbox relative to region origin for easy UI mapping.
        $rel = "{0},{1},{2},{3}" -f ($hx0 - $X0), ($hy0 - $Y0), ($hx1 - $X0), ($hy1 - $Y0)
        return [pscustomobject]@{
          hotCount = $max
          hotPct = [math]::Round($hpct, 3)
          hotBBoxRel = $rel
        }
      }

      # Full-image scan (useful as a sanity check)
      for ($y = 0; $y -lt $h; $y++) {
        $row = $y * $stride
        for ($x = 0; $x -lt $w; $x++) {
          $i = $row + ($x * 4)
          $b = $bytes[$i]
          $g = $bytes[$i + 1]
          $r = $bytes[$i + 2]
          $a = $bytes[$i + 3]

          if ($a -gt 0 -and (($r -gt $Thresh) -or ($g -gt $Thresh) -or ($b -gt $Thresh))) {
            if ($x -lt $minX) { $minX = $x }
            if ($y -lt $minY) { $minY = $y }
            if ($x -gt $maxX) { $maxX = $x }
            if ($y -gt $maxY) { $maxY = $y }

            $sumX += $x
            $sumY += $y
            $count++
          }
        }
      }

      # Diff-panel scan
      $x0 = $region.x0
      $x1 = $region.x1
      $y0 = $region.y0
      $y1 = $region.y1
      $diff = Scan-Region -X0 $x0 -X1 $x1 -Y0 $y0 -Y1 $y1
      $hot = Find-HotBlock -X0 $x0 -X1 $x1 -Y0 $y0 -Y1 $y1 -BlockSize 32

      $hotBBoxSrc = $null
      $diffBBoxRel = $null
      $diffBBoxSrc = $null

      if ($diff.bbox) {
        $db = Parse-BBox -BBox $diff.bbox
        if ($null -ne $db) {
          $diffBBoxRel = "{0},{1},{2},{3}" -f ($db.x0 - $x0), ($db.y0 - $y0), ($db.x1 - $x0), ($db.y1 - $y0)
        }
      }

      if ($region.hasMeta -and $hot.hotBBoxRel) {
        $scale = [double]$region.meta.scale
        $cropX0 = [int]$region.meta.crop.x
        $cropY0 = [int]$region.meta.crop.y

        $hb = Parse-BBox -BBox $hot.hotBBoxRel
        if ($null -ne $hb -and $scale -gt 0) {
          $relMinX = $hb.x0
          $relMinY = $hb.y0
          $relMaxX = $hb.x1
          $relMaxY = $hb.y1

          $cropMinX = [int][math]::Floor($relMinX / $scale)
          $cropMinY = [int][math]::Floor($relMinY / $scale)
          $cropMaxX = [int]([math]::Ceiling(($relMaxX + 1) / $scale) - 1)
          $cropMaxY = [int]([math]::Ceiling(($relMaxY + 1) / $scale) - 1)

          $srcMinX = $cropX0 + $cropMinX
          $srcMinY = $cropY0 + $cropMinY
          $srcMaxX = $cropX0 + $cropMaxX
          $srcMaxY = $cropY0 + $cropMaxY

          $hotBBoxSrc = "{0},{1},{2},{3}" -f $srcMinX, $srcMinY, $srcMaxX, $srcMaxY
        }

        if ($region.hasMeta -and $diffBBoxRel) {
          $dbRel = Parse-BBox -BBox $diffBBoxRel
          if ($null -ne $dbRel -and $scale -gt 0) {
            $cropMinX = [int][math]::Floor($dbRel.x0 / $scale)
            $cropMinY = [int][math]::Floor($dbRel.y0 / $scale)
            $cropMaxX = [int]([math]::Ceiling(($dbRel.x1 + 1) / $scale) - 1)
            $cropMaxY = [int]([math]::Ceiling(($dbRel.y1 + 1) / $scale) - 1)

            $diffBBoxSrc = "{0},{1},{2},{3}" -f ($cropX0 + $cropMinX), ($cropY0 + $cropMinY), ($cropX0 + $cropMaxX), ($cropY0 + $cropMaxY)
          }
        }
      }

      if ($count -eq 0) {
        return [pscustomobject]@{
          file = (Split-Path $Path -Leaf)
          w = $w
          h = $h
          panelW = $region.panelW
          count = 0
          pct = 0
          bbox = $null
          cx = $null
          cy = $null
          diffCount = 0
          diffPct = 0
          diffBBox = $null
          diffBBoxRel = $null
          diffBBoxSrc = $null
          diffCx = $null
          diffCy = $null
          hotCount = 0
          hotPct = 0
          hotBBoxRel = $null
          hotBBoxSrc = $null
          metaScale = $(if ($region.hasMeta) { $region.meta.scale } else { $null })
          metaCrop = $(if ($region.hasMeta) { "{0},{1},{2},{3}" -f $region.meta.crop.x, $region.meta.crop.y, $region.meta.crop.w, $region.meta.crop.h } else { $null })
        }
      }

      $cx = [double]$sumX / $count
      $cy = [double]$sumY / $count
      $pct = 100.0 * $count / ($w * $h)

      return [pscustomobject]@{
        file = (Split-Path $Path -Leaf)
        w = $w
        h = $h
        panelW = $region.panelW
        count = $count
        pct = [math]::Round($pct, 3)
        bbox = "$minX,$minY,$maxX,$maxY"
        cx = [math]::Round($cx, 1)
        cy = [math]::Round($cy, 1)
        diffCount = $diff.count
        diffPct = $diff.pct
        diffBBox = $diff.bbox
        diffBBoxRel = $diffBBoxRel
        diffBBoxSrc = $diffBBoxSrc
        diffCx = $diff.cx
        diffCy = $diff.cy
        hotCount = $hot.hotCount
        hotPct = $hot.hotPct
        hotBBoxRel = $hot.hotBBoxRel
        hotBBoxSrc = $hotBBoxSrc
        metaScale = $(if ($region.hasMeta) { $region.meta.scale } else { $null })
        metaCrop = $(if ($region.hasMeta) { "{0},{1},{2},{3}" -f $region.meta.crop.x, $region.meta.crop.y, $region.meta.crop.w, $region.meta.crop.h } else { $null })
      }
    }
    finally {
      $bmp.UnlockBits($data)
    }
  }
  finally {
    $bmp.Dispose()
  }
}

$ArtifactsDir = (Resolve-Path $ArtifactsDir).Path

$diffFiles = Get-ChildItem -Path $ArtifactsDir -Filter 'diff_*.png' -File | Sort-Object Name
if (-not $diffFiles) {
  Write-Error "No diff_*.png files found in $ArtifactsDir"
}

$results = foreach ($f in $diffFiles) {
  Get-NonBlackBBox -Path $f.FullName -Thresh $Threshold
}

$results |
  Select-Object file, w, h, panelW, diffCount, diffPct, diffBBox, diffBBoxRel, diffBBoxSrc, diffCx, diffCy, hotCount, hotPct, hotBBoxRel, hotBBoxSrc, metaScale, metaCrop |
  ConvertTo-Csv -NoTypeInformation |
  ForEach-Object { Write-Output $_ }
