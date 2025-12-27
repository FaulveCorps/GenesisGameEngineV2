$scriptRoot = $PSScriptRoot
$procdump = Join-Path $scriptRoot '..\tools\Procdump\procdump.exe'
if (-not (Test-Path $procdump)) { Write-Error "ProcDump not found at $procdump"; exit 1 }
$dumpDir = Join-Path $scriptRoot '..\dumps'
New-Item -Path $dumpDir -ItemType Directory -Force | Out-Null
# Prefer explicit SampleGame output path built in Debug
$sampleExe = Join-Path $scriptRoot '..\Build\GameProjects\SampleGame\Debug\SampleGame.exe'
if (-not (Test-Path $sampleExe)) {
    Write-Output "Warning: explicit SampleGame.exe not found at $sampleExe, will rely on PATH / cwd"
    $sampleExe = 'SampleGame.exe'
}
$argList = @('-accepteula','-e','-ma','-x',$dumpDir,$sampleExe,'--stress','100000')
Write-Output "Running: $procdump $($argList -join ' ')"
$proc = Start-Process -FilePath $procdump -ArgumentList $argList -WorkingDirectory (Split-Path $sampleExe) -PassThru
Write-Output "Started ProcDump (PID=$($proc.Id)). Dumps will be written to: $dumpDir"