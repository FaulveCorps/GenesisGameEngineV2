# Rollback script: create backup branch, commit WIP, reset and force-push
$ErrorActionPreference = 'Stop'

$branch = git rev-parse --abbrev-ref HEAD
if ($branch -eq 'HEAD') {
    Write-Host "ERROR: detached HEAD; aborting"
    exit 1
}
$target = 'b65bf210e437a69050793df436c68425d0baf1c0'
if (-not (git rev-list --all | Select-String -SimpleMatch -Quiet $target)) {
    Write-Host "ERROR: commit $target not found"
    exit 1
}
$ts = Get-Date -Format 'yyyyMMdd-HHmmss'
$base_backup = "backup/$branch-before-rollback-$ts"
$backup = $base_backup
$i = 1
while ($true) {
    git show-ref --quiet --verify "refs/heads/$backup"
    if ($LASTEXITCODE -ne 0) { break }
    $backup = "$base_backup-$i"
    $i++
}
Write-Host "Creating backup branch $backup"
git checkout -b $backup
$porcelain = git status --porcelain
if (-not [string]::IsNullOrWhiteSpace($porcelain)) {
    git add -A
    git commit -m "WIP: backup before rolling back to $target on $ts"
    Write-Host "Committed changes to $backup"
} else {
    Write-Host "No uncommitted changes to commit on backup branch"
}
try {
    git push -u origin $backup
    Write-Host "Backup branch pushed to origin/$backup"
} catch {
    Write-Host "Warning: failed to push backup branch to origin: $_"
}
Write-Host "Switching back to original branch $branch"
git checkout $branch
Write-Host "Fetching origin"
git fetch origin
Write-Host "Resetting branch $branch (hard) to $target"
git reset --hard $target
Write-Host "Local HEAD now:"
git rev-parse HEAD | Write-Host
Write-Host "Force-pushing branch $branch to origin"
git push --force-with-lease origin $branch
Write-Host "Rollback complete"
Write-Host "--- summary ---"
git log --oneline -n 5
git status -sb
Write-Host "Backup branch: $backup"
