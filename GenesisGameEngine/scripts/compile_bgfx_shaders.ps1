# Helper: attempt to compile BGFX shaders if a shader compiler is available.
# This script is best-effort: it will detect common shaderc locations and run
# an invocation suitable for many setups. It's non-fatal; if shaderc is not
# available this will print instructions instead.

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = Resolve-Path "$scriptDir\.."
$shadersSrc = Join-Path $repoRoot "assets\shaders\bgfx"
$shadercCandidates = @(
    "$repoRoot\vcpkg\installed\x64-windows\tools\bgfx\shaderc.exe",
    "$repoRoot\vcpkg\installed\x64-windows\tools\shaderc\shaderc.exe",
    "$repoRoot\tools\shaderc\shaderc.exe",
    "$repoRoot\tools\shaderc\bin\shaderc.exe",
    "shaderc.exe",
    "bgfx-shaderc.exe"
)

$shaderc = $null
foreach ($cand in $shadercCandidates) {
    if (Test-Path $cand) { $shaderc = $cand; break }
    try { $found = (Get-Command $cand -ErrorAction SilentlyContinue); if ($found) { $shaderc = $found.Path; break } } catch {}
}

if (-not $shaderc) {
    Write-Host "No shader compiler found (shaderc). To generate bgfx shader binaries, install shaderc or use vcpkg to provide it."
    Write-Host "Example: .\\vcpkg\\vcpkg.exe install bgfx:x64-windows --include-optional" -ForegroundColor Yellow
    exit 0
}

Write-Host "Using shaderc: $shaderc"
$targets = @('d3d11','d3d12','glsl','spirv')
$vert = Join-Path $shadersSrc 'triangle.sc.vert'
$frag = Join-Path $shadersSrc 'triangle.sc.frag'

if (-not (Test-Path $vert) -or -not (Test-Path $frag)) { Write-Error "Shader sources not found under $shadersSrc"; exit 1 }

foreach ($t in $targets) {
    $outdir = Join-Path $shadersSrc $t
    if (-not (Test-Path $outdir)) { New-Item -ItemType Directory -Path $outdir | Out-Null }

    $vsOut = Join-Path $outdir 'vs_triangle.bin'
    $fsOut = Join-Path $outdir 'fs_triangle.bin'

    # Best-effort invocation — shaderc flags differ by distribution. This tries common args.
    Write-Host "Compiling for target: $t -> $outdir"
    & $shaderc -f $vert -o $vsOut -i $shadersSrc -p windows -s vs -v $t 2>&1 | Write-Host
    & $shaderc -f $frag -o $fsOut -i $shadersSrc -p windows -s fs -v $t 2>&1 | Write-Host
}

Write-Host "Done. If compilation produced valid binaries, they will be under $shadersSrc\<target>\" -ForegroundColor Green
