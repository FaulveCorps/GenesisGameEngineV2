param(
    [string]$Unit = "build-vs\tests\Debug\UnitTests.exe",
    [string]$TestFilter = "WasmRuntime RAII: register and invoke host function",
    [int]$Timeout = 60,
    [string]$ProcdumpPath = "",
    [string]$DumpDir = "",
    [string]$SymModule = ""
)

$script = Join-Path $PSScriptRoot 'auto_capture_live_dump.py'
$cmd = "python `"$script`" --unit `"$Unit`" --test `"$TestFilter`" --timeout $Timeout"
if ($ProcdumpPath) { $cmd += " --procdump `"$ProcdumpPath`"" }
if ($DumpDir) { $cmd += " --dumpdir `"$DumpDir`"" }
if ($SymModule) { $cmd += " --sym-module `"$SymModule`"" }
Write-Host "Running: $cmd"
# Start as a child process so console output is visible
iex $cmd
