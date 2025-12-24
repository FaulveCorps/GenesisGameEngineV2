# test_gfx_fallbacks.ps1
# Runs SampleGame with different gfx-order permutations and with/without --gfx-strict
param(
    [string]$exe = "build\GameProjects\SampleGame\Debug\SampleGame.exe",
    [int]$timeoutSec = 10
)
$workspace = Split-Path -Parent $MyInvocation.MyCommand.Definition | Split-Path -Parent
$exePath = Join-Path $workspace $exe
if (-not (Test-Path $exePath)) { Write-Error "Executable not found: $exePath"; exit 2 }

$orders = @(
    'Vulkan,OpenGL',
    'OpenGL,Vulkan',
    'DirectX,OpenGL',
    'OpenGL,DirectX',
    'Vulkan,DirectX,OpenGL'
)

function Run-Case($order, $strict) {
    $argsArray = @("--gfx-order", $order)
    if ($strict) { $argsArray += "--gfx-strict" }
    $argsSummary = $argsArray -join ' '
    Write-Output "\n--- Running: $argsSummary ---"
    $start = Get-Date
    # Run and capture output for a short time
    $proc = Start-Process -FilePath $exePath -ArgumentList $argsArray -RedirectStandardOutput "out.log" -RedirectStandardError "err.log" -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds $timeoutSec
    if (-not $proc.HasExited) {
        try { $proc.Kill() } catch {};
        Write-Output "Process killed after timeout ($timeoutSec)s"
    }
    $out = Get-Content "out.log" -ErrorAction SilentlyContinue
    $err = Get-Content "err.log" -ErrorAction SilentlyContinue
    # Find selection lines
    $sel = $out | Select-String "GraphicsFactory: selected|skipping VulkanRenderer|VulkanRenderer: Win32 fallback|no swapchain|GraphicsFactory: skipping VulkanRenderer|GraphicsFactory: selected" -AllMatches
    if ($sel) { $sel | ForEach-Object { Write-Output $_.ToString() } } else { Write-Output "No selection-specific messages found (see out.log)" }
    Write-Output "Exit snippet:"
    $out | Select-Object -Last 8 | ForEach-Object { Write-Output "  $_" }
}

foreach ($o in $orders) {
    Run-Case $o $false
    Run-Case $o $true
}

Write-Output "\nTest finished"
