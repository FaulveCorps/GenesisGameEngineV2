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

# Optional: Vulkan triangle smoke test (force swapchain + present a CPU-rasterized triangle)
Write-Output "\n--- Vulkan triangle smoke test ---"
$args = @("--gfx-order","Vulkan,OpenGL","--force-vulkan-swapchain","--vulkan-triangle")
Write-Output "Running: $($args -join ' ')"
$proc = Start-Process -FilePath $exePath -ArgumentList $args -RedirectStandardOutput "out.log" -RedirectStandardError "err.log" -WindowStyle Hidden -PassThru
Start-Sleep -Seconds $timeoutSec
if (-not $proc.HasExited) { try { $proc.Kill() } catch {}; Write-Output "Vulkan triangle process killed after timeout ($timeoutSec)s" }
$out = Get-Content "out.log" -ErrorAction SilentlyContinue
$sel = $out | Select-String "VulkanRenderer: host triangle image populated|VulkanRenderer: swapchain created|VulkanRenderer: Win32 fallback - forcing swapchain" -AllMatches
if ($sel) { $sel | ForEach-Object { Write-Output $_.ToString() } } else { Write-Output "No triangle-specific messages found (see out.log)" }
Write-Output "Exit snippet:"
$out | Select-Object -Last 8 | ForEach-Object { Write-Output "  $_" }

Write-Output "\nTest finished"
