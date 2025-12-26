# Check for nested .git directories (PowerShell)
$root = Get-Location
$found = Get-ChildItem -Path $root -Directory -Force -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.Name -eq '.git' -and $_.FullName -ne (Join-Path $root '.git') }
if ($found) {
    Write-Error "Nested git repo(s) found:"; $found | ForEach-Object { Write-Error $_.FullName }
    Exit 2
}
Write-Host "No nested .git directories found."