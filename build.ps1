# Genesis Game Engine - Fast Build Script
# Usage: .\build.ps1 [debug|release|clean|install|test] [--ninja|--vs] [--verbose]

param(
    [Parameter(Position=0)]
    [ValidateSet('debug', 'release', 'clean', 'install', 'test', 'all')]
    [string]$Target = 'debug',
    
    [Parameter(Position=1)]
    [ValidateSet('--ninja', '--vs')]
    [string]$Generator = '--ninja',
    
    [switch]$VerboseOutput,
    [switch]$Help
)

# Colors for output
$colors = @{
    Success = 'Green'
    Error = 'Red'
    Warning = 'Yellow'
    Info = 'Cyan'
}

function Write-Info { Write-Host "$args" -ForegroundColor $colors.Info }
function Write-Success { Write-Host "$args" -ForegroundColor $colors.Success }
function Write-Error { Write-Host "ERROR: $args" -ForegroundColor $colors.Error }
function Write-Warning { Write-Host "WARNING: $args" -ForegroundColor $colors.Warning }

function Show-Help {
    @"
Genesis Game Engine - Fast Build Script

USAGE:
    .\build.ps1 [target] [generator] [options]

TARGETS:
    debug       Build debug configuration (default)
    release     Build release configuration with optimizations
    all         Build both debug and release
    clean       Remove all build artifacts
    install     Build and install to output directory
    test        Build and run unit tests

GENERATORS:
    --ninja     Use Ninja Multi-Config (default, faster)
    --vs        Use Visual Studio 2022 (IDE support)

OPTIONS:
    --verbose   Show detailed build output
    --help      Show this help message

EXAMPLES:
    .\build.ps1                    # Fast debug build with Ninja
    .\build.ps1 release            # Optimized release build
    .\build.ps1 debug --ninja      # Debug with Ninja (explicit)
    .\build.ps1 all                # Build both configurations
    .\build.ps1 test               # Run unit tests
    .\build.ps1 clean --ninja      # Clean Ninja build artifacts

TIMING:
    Ninja (debug):     ~30-45 seconds (first build)
                       ~5-10 seconds (incremental)
    Ninja (release):   ~60-90 seconds (optimizations)
    VS2022:            ~45-60 seconds (first build)
                       ~8-15 seconds (incremental)
"@
}

if ($Help) {
    Show-Help
    exit 0
}

# Determine build directory and preset
$buildDir = if ($Generator -eq '--vs') { 'build-vs' } else { "build-ninja-$Target" }
if ($Target -eq 'debug' -or $Target -eq 'all') {
    $buildDir = 'build-ninja-debug'
}

$presetName = if ($Generator -eq '--vs') { 'vs2022' } else { 
    if ($Target -eq 'debug') { 'ninja-debug' } 
    elseif ($Target -eq 'release') { 'ninja-release' } 
    else { 'ninja-debug' } 
}

# Verify Ninja is installed
if ($Generator -eq '--ninja') {
    $ninjaPath = Get-Command ninja -ErrorAction SilentlyContinue
    if (-not $ninjaPath) {
        Write-Error "Ninja not found. Install with: choco install ninja"
        exit 1
    }
    Write-Info "Ninja found: $($ninjaPath.Source)"
}

# Verify CMake is installed
$cmakePath = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakePath) {
    Write-Error "CMake not found. Install with: choco install cmake"
    exit 1
}
Write-Info "CMake found: $($cmakePath.Source)"

$sourceDir = Get-Location
Write-Info "Project directory: $sourceDir"
Write-Info ""

# ============================================================================
# CONFIGURE
# ============================================================================
function Invoke-Configure {
    param([string]$Preset, [string]$BuildDir)
    
    Write-Info "=========================================="
    Write-Info "CONFIGURING with preset: $Preset"
    Write-Info "=========================================="
    
    $startTime = Get-Date
    if ($VerboseOutput) {
        cmake --preset $Preset
    } else {
        cmake --preset $Preset
    }
    $elapsed = (Get-Date) - $startTime
    
    if ($LASTEXITCODE -eq 0) {
        Write-Success "Configuration complete in $($elapsed.TotalSeconds)s"
        return $true
    } else {
        Write-Error "Configuration failed"
        return $false
    }
}

# ============================================================================
# BUILD
# ============================================================================
function Invoke-Build {
    param([string]$Preset, [string]$Config, [string]$Target)
    
    Write-Info "=========================================="
    Write-Info "BUILDING $Config"
    Write-Info "=========================================="
    
    $startTime = Get-Date
    if ($VerboseOutput) {
        cmake --build --preset $Preset --config $Config $(if ($Target -and $Target -ne 'all') { '--target'; $Target }) --verbose
    } else {
        cmake --build --preset $Preset --config $Config $(if ($Target -and $Target -ne 'all') { '--target'; $Target })
    }
    $elapsed = (Get-Date) - $startTime
    
    if ($LASTEXITCODE -eq 0) {
        Write-Success "Build complete in $($elapsed.TotalSeconds)s"
        return $true
    } else {
        Write-Error "Build failed"
        return $false
    }
}

# ============================================================================
# RUN TESTS
# ============================================================================
function Invoke-Tests {
    param([string]$BuildDir)
    Write-Info "=========================================="
    Write-Info "RUNNING UNIT TESTS"
    Write-Info "=========================================="
    
    $startTime = Get-Date
    ctest -C Debug --test-dir $BuildDir --output-on-failure $(if ($VerboseOutput) { '-V' })
    $elapsed = (Get-Date) - $startTime
    
    if ($LASTEXITCODE -eq 0) {
        Write-Success "All tests passed in $($elapsed.TotalSeconds)s"
        return $true
    } else {
        Write-Error "Some tests failed"
        return $false
    }
}

# ============================================================================
# CLEAN
# ============================================================================
function Invoke-Clean {
    Write-Info "=========================================="
    Write-Info "CLEANING BUILD ARTIFACTS"
    Write-Info "=========================================="
    
    $buildDirs = @('build', 'build-ninja-debug', 'build-ninja-release', 'build-vs')
    $removed = 0
    
    foreach ($dir in $buildDirs) {
        $path = Join-Path $sourceDir $dir
        if (Test-Path $path) {
            Write-Info "Removing: $dir"
            Remove-Item -Recurse -Force $path -ErrorAction SilentlyContinue
            $removed++
        }
    }
    
    Write-Success "Cleaned $removed build directories"
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

# Determine preset based on generator
$debugPreset = if ($Generator -eq '--vs') { 'vs2022' } else { 'ninja-debug' }
$releasePreset = if ($Generator -eq '--vs') { 'vs2022' } else { 'ninja-release' }
$debugBuildDir = if ($Generator -eq '--vs') { 'build-vs' } else { 'build-ninja-debug' }
$releaseBuildDir = if ($Generator -eq '--vs') { 'build-vs' } else { 'build-ninja-release' }

Write-Host ""
Write-Info "Generator: $(if ($Generator -eq '--vs') { 'Visual Studio 2022' } else { 'Ninja Multi-Config' })"
Write-Host ""

switch ($Target) {
    'clean' {
        Invoke-Clean
    }
    
    'debug' {
        if (-not (Invoke-Configure $debugPreset $debugBuildDir)) { exit 1 }
        if (-not (Invoke-Build $debugPreset 'Debug')) { exit 1 }
        Write-Success "Debug build complete!"
    }
    
    'release' {
        if (-not (Invoke-Configure $releasePreset $releaseBuildDir)) { exit 1 }
        if (-not (Invoke-Build $releasePreset 'Release')) { exit 1 }
        Write-Success "Release build complete!"
    }
    
    'all' {
        if (-not (Invoke-Configure $debugPreset $debugBuildDir)) { exit 1 }
        if (-not (Invoke-Build $debugPreset 'Debug')) { exit 1 }
        Write-Success "Debug build complete!"
        
        Write-Info ""
        if (-not (Invoke-Configure $releasePreset $releaseBuildDir)) { exit 1 }
        if (-not (Invoke-Build $releasePreset 'Release')) { exit 1 }
        Write-Success "Release build complete!"
    }
    
    'test' {
        if (-not (Invoke-Configure $debugPreset $debugBuildDir)) { exit 1 }
        if (-not (Invoke-Build $debugPreset 'Debug')) { exit 1 }
        Write-Info ""
        if (-not (Invoke-Tests $debugBuildDir)) { exit 1 }
    }
    
    'install' {
        if (-not (Invoke-Configure $releasePreset $releaseBuildDir)) { exit 1 }
        if (-not (Invoke-Build $releasePreset 'Release')) { exit 1 }
        Write-Info "Installation would proceed here (CPack)"
    }
}

Write-Success "=========================================="
Write-Success "Build script completed successfully"
Write-Success "=========================================="
