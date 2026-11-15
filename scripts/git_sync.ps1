# ==============================================================================
# Team-04 Git Sync Utility (GitHub + GitLab)
# Usage:
#   .\scripts\git_sync.ps1 "feat: your commit message"
#   .\scripts\git_sync.ps1 "feat: your commit message" -ToMain
# ==============================================================================

param (
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Message,

    [switch]$ToMain
)

$ErrorActionPreference = "Stop"

Write-Host "=== Team-04 Git Sync ===" -ForegroundColor Cyan

# 1. Ensure git settings for Windows
git config core.autocrlf true
git config core.quotepath false

# 2. Check status
Write-Host "`n[1/4] Checking status..." -ForegroundColor Yellow
git status --short

# 3. Add and commit
Write-Host "`n[2/4] Committing changes..." -ForegroundColor Yellow
git add -A
git commit -m "$Message"

# 4. Push to GitHub
Write-Host "`n[3/4] Pushing to GitHub (origin/developV2)..." -ForegroundColor Yellow
git push origin developV2

# 5. Push to GitLab
Write-Host "`n[4/4] Pushing to GitLab (gitlab/developv2)..." -ForegroundColor Yellow
git push gitlab developV2:developv2
git push gitlab developV2:developV2

# 6. Optional MR to main on GitLab
if ($ToMain) {
    Write-Host "`n[+] Creating GitLab Merge Request to main..." -ForegroundColor Magenta
    git push -o merge_request.create -o merge_request.target=main -o merge_request.title="$Message" gitlab developV2:mr-to-main
}

Write-Host "`n[SUCCESS] Sync complete!" -ForegroundColor Green
