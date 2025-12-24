# run_stress.ps1
# Repeatedly runs SampleGame in stress mode until a non-zero exit code (crash) is observed
param(
    [string]$exe = "build\GameProjects\SampleGame\Debug\SampleGame.exe",
    [int]$frames = 5000,
    [int]$iterations = 0 # 0 == infinite
)
$workspace = Split-Path -Parent $MyInvocation.MyCommand.Definition | Split-Path -Parent
$exePath = Join-Path $workspace $exe
if (-not (Test-Path $exePath)) { Write-Error "Executable not found: $exePath"; exit 1 }
$iter = 0
while ($true) {
    $iter++
    Write-Output ("Starting iteration {0}: {1} --stress {2}" -f $iter, $exePath, $frames)
    & "$exePath" --stress $frames
    $rc = $LASTEXITCODE
    Write-Output "Iteration $iter finished with exit code $rc"
    if ($rc -ne 0) { Write-Output "Non-zero exit code detected, stopping"; exit $rc }
    if ($iterations -gt 0 -and $iter -ge $iterations) { Write-Output "Completed requested iterations ($iterations)", exit 0 }
}
