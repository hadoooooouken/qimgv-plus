<#
.SYNOPSIS
    Build a custom qtiff.dll with JPEG compression support for Photoshop TIFF files.

.DESCRIPTION
    Builds a 4-step static dependency chain:
      1. zlib         (static)
      2. libjpeg-turbo (static, NASM SIMD)
      3. libtiff       (static, JPEG + Old-JPEG enabled)
      4. qtiff.dll     (Qt image format plugin)

    The resulting qtiff.dll is a drop-in replacement for the stock Qt TIFF plugin,
    with full support for JPEG-compressed TIFF files (Photoshop compatibility).

.PARAMETER Steps
    Which steps to build. Default = all four.
    Example: .\build_qtiff_jpeg.ps1 -Steps libjpeg-turbo,libtiff,qtiff

.PARAMETER FullClean
    Remove entire build folders before building (otherwise only CMakeCache.txt is cleared).

.PARAMETER SkipDeploy
    Skip deploying qtiff.dll into the Qt SDK directory.

.EXAMPLE
    .\build_qtiff_jpeg.ps1
    .\build_qtiff_jpeg.ps1 -Steps qtiff
    .\build_qtiff_jpeg.ps1 -FullClean
#>
param(
    [string[]] $Steps = @("zlib", "libjpeg-turbo", "libtiff", "qtiff"),
    [switch]   $FullClean,
    [switch]   $SkipDeploy
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
$ROOT        = (Resolve-Path "$PSScriptRoot\..").Path
$FORMATS_DIR = Join-Path $ROOT "formats"
$SCRIPT_DIR  = $PSScriptRoot

$QT_DIR      = "E:\Qt\6.11.1\msvc2022_64"
$QT_SDK_IMAGEFORMATS = Join-Path $QT_DIR "plugins\imageformats"

$NASM_EXE    = Join-Path $FORMATS_DIR "nasm\nasm.exe"

# Install prefixes for each dependency
$ZLIB_SRC_DIR     = Join-Path $FORMATS_DIR "zlib-ng"
$ZLIB_INSTALL     = Join-Path $FORMATS_DIR "zlib-ng\install"
$ZLIB_NG_DIR      = Join-Path $FORMATS_DIR "zlib-ng"
$JPEG_INSTALL     = Join-Path $FORMATS_DIR "libjpeg-turbo\install"
$TIFF_INSTALL     = Join-Path $FORMATS_DIR "libtiff\install"
$QTIFF_BUILD_DIR  = Join-Path $SCRIPT_DIR "qtiff_jpeg\build"
$QTIFF_INSTALL    = Join-Path $SCRIPT_DIR "qtiff_jpeg\install"

# ---------------------------------------------------------------------------
# Common compiler / linker hardening flags (match rebuild-all.ps1)
# ---------------------------------------------------------------------------
$C_FLAGS_RELEASE   = "/arch:AVX2 /MD /O2 /Ob2 /Oi /Ot /DNDEBUG /GS /guard:cf /Qspectre"
$CXX_FLAGS_RELEASE = "/arch:AVX2 /MD /O2 /Ob2 /Oi /Ot /DNDEBUG /GS /guard:cf /EHsc /Qspectre"
$LINKER_FLAGS      = "/guard:cf /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /CETCOMPAT"

function Get-HardeningArgs {
    return @(
        "-DCMAKE_C_FLAGS_RELEASE=$C_FLAGS_RELEASE",
        "-DCMAKE_CXX_FLAGS_RELEASE=$CXX_FLAGS_RELEASE",
        "-DCMAKE_SHARED_LINKER_FLAGS_RELEASE=$LINKER_FLAGS",
        "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=$LINKER_FLAGS",
        "-DCMAKE_STATIC_LINKER_FLAGS_RELEASE="
    )
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Write-Header($n, $total, $name) {
    Write-Host ""
    Write-Host ("=" * 60) -ForegroundColor DarkCyan
    Write-Host "  [$n/$total]  $name" -ForegroundColor Cyan
    Write-Host ("=" * 60) -ForegroundColor DarkCyan
}

function Write-OK($msg)   { Write-Host "  [OK]  $msg" -ForegroundColor Green  }
function Write-Info($msg) { Write-Host "  [ ]   $msg" -ForegroundColor Gray   }
function Write-Warn($msg) { Write-Host "  [!]   $msg" -ForegroundColor Yellow }

function Clear-BuildDir($buildDir) {
    if (Test-Path $buildDir) {
        if ($FullClean) {
            Remove-Item $buildDir -Recurse -Force -ErrorAction SilentlyContinue
            Write-Info "Full clean: removed $buildDir"
        } else {
            $cache = Join-Path $buildDir "CMakeCache.txt"
            if (Test-Path $cache) {
                Remove-Item $cache -Force
                Write-Info "Cleared CMakeCache.txt"
            }
        }
    } else {
        Write-Info "No cache found -- fresh build"
    }
}

function Invoke-CMake {
    param([string[]]$CmakeArgs)
    Write-Info "cmake $($CmakeArgs -join ' ')"
    & cmake @CmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake failed (exit code $LASTEXITCODE)"
    }
}

# ---------------------------------------------------------------------------
# Step 1: zlib (static)
# ---------------------------------------------------------------------------
function Build-Zlib {
    $buildDir = Join-Path $ZLIB_SRC_DIR "build_msvc"
    Clear-BuildDir $buildDir

    Write-Info "Configuring zlib-ng (compat)..."
    $args = @(
        "-S", $ZLIB_SRC_DIR,
        "-B", $buildDir,
        "-A", "x64",
        "-DCMAKE_INSTALL_PREFIX=$ZLIB_INSTALL",
        "-DBUILD_SHARED_LIBS=ON",
        "-DZLIB_COMPAT=ON",
        "-DZLIB_ENABLE_TESTS=OFF",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"
    ) + (Get-HardeningArgs)
    Invoke-CMake $args

    Write-Info "Building zlib-ng..."
    Invoke-CMake @("--build", $buildDir, "--config", "Release", "--parallel")

    Write-Info "Installing zlib-ng..."
    Invoke-CMake @("--install", $buildDir, "--config", "Release")

    Write-OK "zlib-ng (compat) installed to $ZLIB_INSTALL"
}

# ---------------------------------------------------------------------------
# Step 2: libjpeg-turbo (static, NASM SIMD)
# ---------------------------------------------------------------------------
function Build-LibjpegTurbo {
    $srcDir   = Join-Path $FORMATS_DIR "libjpeg-turbo"
    $buildDir = Join-Path $srcDir "build_msvc"

    if (-not (Test-Path $NASM_EXE)) {
        throw "NASM not found at $NASM_EXE"
    }

    Clear-BuildDir $buildDir

    Write-Info "Configuring libjpeg-turbo with NASM SIMD..."
    $args = @(
        "-S", $srcDir,
        "-B", $buildDir,
        "-A", "x64",
        "-DCMAKE_INSTALL_PREFIX=$JPEG_INSTALL",
        "-DENABLE_SHARED=OFF",
        "-DENABLE_STATIC=ON",
        "-DWITH_SIMD=ON",
        "-DWITH_CRT_DLL=ON",
        "-DWITH_TURBOJPEG=OFF",
        "-DCMAKE_ASM_NASM_COMPILER=$NASM_EXE",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"
    ) + (Get-HardeningArgs)
    Invoke-CMake $args

    Write-Info "Building libjpeg-turbo..."
    Invoke-CMake @("--build", $buildDir, "--config", "Release", "--parallel")

    Write-Info "Installing libjpeg-turbo..."
    Invoke-CMake @("--install", $buildDir, "--config", "Release")

    Write-OK "libjpeg-turbo installed to $JPEG_INSTALL"
}

# ---------------------------------------------------------------------------
# Step 3: libtiff (static, JPEG + Old-JPEG)
# ---------------------------------------------------------------------------
function Build-Libtiff {
    $srcDir   = Join-Path $FORMATS_DIR "libtiff"
    $buildDir = Join-Path $srcDir "build_msvc"

    # CMAKE_PREFIX_PATH lets libtiff's find_package(ZLIB) and find_package(JPEG) work
    $prefixPath = "$ZLIB_INSTALL;$JPEG_INSTALL"

    Clear-BuildDir $buildDir

    Write-Info "Configuring libtiff with JPEG support..."
    $args = @(
        "-S", $srcDir,
        "-B", $buildDir,
        "-A", "x64",
        "-DCMAKE_INSTALL_PREFIX=$TIFF_INSTALL",
        "-DCMAKE_PREFIX_PATH=$prefixPath",
        "-DBUILD_SHARED_LIBS=OFF",
        "-Djpeg=ON",
        "-Dold-jpeg=ON",
        "-Dtiff-tools=OFF",
        "-Dtiff-tests=OFF",
        "-Dtiff-contrib=OFF",
        "-Dtiff-docs=OFF",
        "-Dtiff-install=ON",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"
    ) + (Get-HardeningArgs)
    Invoke-CMake $args

    Write-Info "Building libtiff..."
    Invoke-CMake @("--build", $buildDir, "--config", "Release", "--parallel")

    Write-Info "Installing libtiff..."
    Invoke-CMake @("--install", $buildDir, "--config", "Release")

    Write-OK "libtiff installed to $TIFF_INSTALL"
}

# ---------------------------------------------------------------------------
# Step 4: qtiff.dll (Qt plugin)
# ---------------------------------------------------------------------------
function Build-Qtiff {
    $srcDir = Join-Path $SCRIPT_DIR "qtiff_jpeg"

    # PREFIX_PATH must include Qt, libtiff, libjpeg-turbo, and zlib installs
    # so that find_package(Qt6), find_package(tiff), find_package(JPEG),
    # and find_package(ZLIB) all succeed.
    $prefixPath = "$QT_DIR;$TIFF_INSTALL;$JPEG_INSTALL;$ZLIB_INSTALL"

    Clear-BuildDir $QTIFF_BUILD_DIR

    Write-Info "Configuring qtiff plugin..."
    $args = @(
        "-S", $srcDir,
        "-B", $QTIFF_BUILD_DIR,
        "-A", "x64",
        "-DCMAKE_INSTALL_PREFIX=$QTIFF_INSTALL",
        "-DCMAKE_PREFIX_PATH=$prefixPath",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
        "-DZLIB_LIBRARY=$ZLIB_INSTALL\lib\zlib.lib",
        "-DZLIB_INCLUDE_DIR=$ZLIB_INSTALL\include"
    ) + (Get-HardeningArgs)
    Invoke-CMake $args

    Write-Info "Building qtiff.dll..."
    Invoke-CMake @("--build", $QTIFF_BUILD_DIR, "--config", "Release", "--parallel")

    Write-Info "Installing qtiff.dll..."
    Invoke-CMake @("--install", $QTIFF_BUILD_DIR, "--config", "Release")

    Write-OK "qtiff.dll installed to $QTIFF_INSTALL"
}

# ---------------------------------------------------------------------------
# Deploy: replace stock qtiff.dll in Qt SDK (with backup)
# ---------------------------------------------------------------------------
function Deploy-Qtiff {
    $customDll = Join-Path $QTIFF_INSTALL "qtiff.dll"
    $stockDll  = Join-Path $QT_SDK_IMAGEFORMATS "qtiff.dll"
    $backupDll = Join-Path $QT_SDK_IMAGEFORMATS "qtiff.dll.stock"

    if (-not (Test-Path $customDll)) {
        Write-Warn "Custom qtiff.dll not found at $customDll -- skipping deploy"
        return
    }

    # Back up original only if not already backed up
    if ((Test-Path $stockDll) -and -not (Test-Path $backupDll)) {
        Copy-Item $stockDll $backupDll -Force
        Write-Info "Backed up stock qtiff.dll -> qtiff.dll.stock"
    }

    Copy-Item $customDll $stockDll -Force
    Write-OK "Deployed custom qtiff.dll to Qt SDK: $stockDll"

    # Also copy to project release directory if it exists
    $releaseDll = Join-Path $ROOT "release\imageformats\qtiff.dll"
    $releaseDir = Split-Path $releaseDll -Parent
    if (Test-Path $releaseDir) {
        Copy-Item $customDll $releaseDll -Force
        Write-OK "Deployed custom qtiff.dll to release: $releaseDll"
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

$sw      = [System.Diagnostics.Stopwatch]::StartNew()
$failed  = @()
$built   = @()
$total   = $Steps.Count
$n       = 0

$ALL_STEPS = [ordered]@{
    "zlib"           = { Build-Zlib }
    "libjpeg-turbo"  = { Build-LibjpegTurbo }
    "libtiff"        = { Build-Libtiff }
    "qtiff"          = { Build-Qtiff }
}

Write-Host ""
Write-Host "  qtiff.dll JPEG build" -ForegroundColor White
Write-Host "  Root : $ROOT"         -ForegroundColor DarkGray
Write-Host "  Steps: $($Steps -join ', ')" -ForegroundColor DarkGray
Write-Host "  NASM : $NASM_EXE"    -ForegroundColor DarkGray
Write-Host ""

foreach ($step in $Steps) {
    $n++
    if (-not $ALL_STEPS.Contains($step)) {
        Write-Header $n $total $step
        Write-Warn "Unknown step '$step' -- skipping."
        continue
    }

    Write-Header $n $total $step
    try {
        & $ALL_STEPS[$step]
        $built += $step
    } catch {
        Write-Host "  [FAIL]  $_" -ForegroundColor Red
        $failed += $step
    }
}

# Deploy if all steps succeeded
if ($failed.Count -eq 0 -and -not $SkipDeploy -and $Steps -contains "qtiff") {
    Write-Host ""
    Write-Host ("=" * 60) -ForegroundColor DarkCyan
    Write-Host "  Deploy" -ForegroundColor Cyan
    Write-Host ("=" * 60) -ForegroundColor DarkCyan
    try {
        Deploy-Qtiff
    } catch {
        Write-Host "  [FAIL]  Deploy: $_" -ForegroundColor Red
        $failed += "deploy"
    }
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

$sw.Stop()
$elapsed = "{0:mm\:ss}" -f $sw.Elapsed

Write-Host ""
Write-Host ("=" * 60) -ForegroundColor DarkCyan
Write-Host "  Summary  ($elapsed)" -ForegroundColor Cyan
Write-Host ("=" * 60) -ForegroundColor DarkCyan

if ($built)  { Write-Host "  Built : $($built  -join ', ')" -ForegroundColor Green }
if ($failed) { Write-Host "  Failed: $($failed -join ', ')" -ForegroundColor Red   }
Write-Host ""

if ($failed) { exit 1 }
