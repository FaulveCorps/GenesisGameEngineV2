$scriptRoot = $PSScriptRoot
$procdump = Join-Path $scriptRoot '..\tools\Procdump\procdump.exe'
if (-not (Test-Path $procdump)) { Write-Error "ProcDump not found at $procdump"; exit 1 }
$dumpDir = Join-Path $scriptRoot '..\dumps\forced_vulkan'
New-Item -Path $dumpDir -ItemType Directory -Force | Out-Null
$sampleExe = Join-Path $scriptRoot '..\Build\GameProjects\SampleGame\Debug\SampleGame.exe'
if (-not (Test-Path $sampleExe)) { Write-Error "SampleGame not found at $sampleExe"; exit 1 }
# Set env var for this process
$env:GENESIS_FORCE_VULKAN_SWAPCHAIN = '1'
# Run procDump to start the process; pass a short stress count to limit run
$argList = @('-accepteula','-e','-ma','-x',$dumpDir,$sampleExe,'--gfx-order','Vulkan,OpenGL','--stress','100')
Write-Output "Running: $procdump $($argList -join ' ')"
$proc = Start-Process -FilePath $procdump -ArgumentList $argList -WorkingDirectory (Split-Path $sampleExe) -PassThru
Write-Output "Started ProcDump (PID=$($proc.Id)). Dumps will be written to: $dumpDir"