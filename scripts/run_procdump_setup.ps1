$tools = Join-Path $PSScriptRoot '..\tools'
New-Item -ItemType Directory -Path $tools -Force | Out-Null
$zip = Join-Path $tools 'Procdump.zip'
$uri = 'https://download.sysinternals.com/files/Procdump.zip'
Write-Output "Downloading $uri to $zip"
Invoke-WebRequest -Uri $uri -OutFile $zip -UseBasicParsing
$dest = Join-Path $tools 'Procdump'
Write-Output "Extracting $zip to $dest"
Expand-Archive -Path $zip -DestinationPath $dest -Force
$pd = Get-ChildItem $dest -Filter procdump.exe -Recurse | Select-Object -First 1
if ($pd) { Write-Output "Found: $($pd.FullName)" } else { Write-Error "ProcDump not found"; exit 1 }