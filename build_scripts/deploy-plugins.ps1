# deploy-plugins.ps1
# Copies built kimageformats plugins to release/imageformats/
# Run from project root (e:\qimgv)

$ErrorActionPreference = "Stop"

$projectRoot = $PSScriptRoot
$buildDir    = Join-Path $projectRoot "out\build\qimgv-vs\imageformats\Release"
$releaseDir  = Join-Path $projectRoot "release\imageformats"

if (-not (Test-Path $buildDir)) {
    Write-Host "Build directory not found: $buildDir" -ForegroundColor Red
    Write-Host "Make sure you've built the project first." -ForegroundColor Yellow
    exit 1
}

# Create destination if missing
if (-not (Test-Path $releaseDir)) {
    New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
}

$dlls = Get-ChildItem -Path $buildDir -Filter "kimg_*.dll"

if ($dlls.Count -eq 0) {
    Write-Host "No kimg_*.dll files found in: $buildDir" -ForegroundColor Yellow
    exit 1
}

$copied = 0
foreach ($dll in $dlls) {
    $dest = Join-Path $releaseDir $dll.Name
    $needsCopy = (-not (Test-Path $dest)) -or ($dll.LastWriteTime -gt (Get-Item $dest).LastWriteTime)

    if ($needsCopy) {
        Copy-Item $dll.FullName -Destination $dest -Force
        Write-Host "  Copied: $($dll.Name)" -ForegroundColor Green
        $copied++
    }
}

if ($copied -eq 0) {
    Write-Host "All $($dlls.Count) plugins are up to date." -ForegroundColor Cyan
} else {
    Write-Host "Deployed $copied / $($dlls.Count) plugins." -ForegroundColor Green
}
