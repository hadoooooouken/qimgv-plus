# deploy-plugins.ps1
# Copies built kimageformats plugins to release/imageformats/
# Run from project root (e:\qimgv)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$buildDir    = Join-Path $projectRoot "out\build\qimgv-vs\imageformats\Release"
if (-not (Test-Path $buildDir) -or (Get-ChildItem -Path $buildDir -Filter "kimg_*.dll" -ErrorAction SilentlyContinue).Count -eq 0) {
    $buildDir = Join-Path $projectRoot "out\build\qimgv-x64-release\imageformats"
}
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

# Copy EXR/Imath dependency DLLs to main release/ directory
$mainReleaseDir = Join-Path $projectRoot "release"
if (-not (Test-Path $mainReleaseDir)) {
    New-Item -ItemType Directory -Path $mainReleaseDir -Force | Out-Null
}

$depDlls = @(
    "formats\openexr\install\bin\OpenEXR-3_4.dll"
    "formats\openexr\install\bin\OpenEXRCore-3_4.dll"
    "formats\openexr\install\bin\Iex-3_4.dll"
    "formats\openexr\install\bin\IlmThread-3_4.dll"
    "formats\openexr\install\bin\OpenEXRUtil-3_4.dll"
    "formats\Imath\install\bin\Imath-3_2.dll"
    "formats\zlib-ng\install\bin\zlib1.dll"
    "formats\libdeflate\install\bin\deflate.dll"
)

# libjpeg-turbo's shared DLL is versioned (e.g. jpeg62.dll) by SO_MAJOR_VERSION,
# so resolve it by pattern instead of hardcoding the name (unlike the fixed-name
# DLLs above). Shared by qtiff.dll (via libtiff) and qjpeg.dll.
$jpegDll = Get-ChildItem -Path (Join-Path $projectRoot "formats\libjpeg-turbo\install\bin") `
    -Filter "jpeg*.dll" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notmatch "^turbojpeg" } | Select-Object -First 1
if ($jpegDll) {
    $depDlls += "formats\libjpeg-turbo\install\bin\$($jpegDll.Name)"
} else {
    Write-Warning "Shared libjpeg-turbo DLL not found under formats\libjpeg-turbo\install\bin"
}

Write-Host "Deploying EXR dependency DLLs..." -ForegroundColor Cyan
foreach ($relPath in $depDlls) {
    $srcPath = Join-Path $projectRoot $relPath
    if (Test-Path $srcPath) {
        $dllFile = Get-Item $srcPath
        $destPath = Join-Path $mainReleaseDir $dllFile.Name
        
        $needsDepCopy = (-not (Test-Path $destPath)) -or ($dllFile.LastWriteTime -gt (Get-Item $destPath).LastWriteTime)
        if ($needsDepCopy) {
            Copy-Item $srcPath -Destination $destPath -Force
            Write-Host "  Copied Dependency: $($dllFile.Name)" -ForegroundColor Green
        } else {
            Write-Host "  Up to date: $($dllFile.Name)" -ForegroundColor DarkGreen
        }
    } else {
        Write-Warning "Dependency DLL not found: $srcPath"
    }
}

