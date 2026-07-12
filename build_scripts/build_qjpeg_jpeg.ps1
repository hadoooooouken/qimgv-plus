<#
.SYNOPSIS
    Build a custom qjpeg.dll backed by the shared, AVX2/LTCG libjpeg-turbo.

.DESCRIPTION
    Single-step build:
      1. qjpeg.dll  (Qt JPEG image format plugin)

    Reuses the SHARED libjpeg-turbo already built by build_qtiff_jpeg.ps1
    (formats/libjpeg-turbo/install, Step "libjpeg-turbo") -- it is not
    rebuilt here. Run build_qtiff_jpeg.ps1 at least through that step first:

        .\build_qtiff_jpeg.ps1 -Steps libjpeg-turbo

    The resulting qjpeg.dll is a drop-in replacement for the stock Qt JPEG
    plugin, decoding/encoding plain .jpg/.jpeg files through the same
    AVX2-optimized codec that qtiff.dll (via libtiff) already uses for
    JPEG-compressed TIFF -- one copy of libjpeg-turbo on disk and in memory
    instead of two.

    Unlike kimg_png (built as part of the main qimgv-plus CMake tree), this
    plugin follows the qtiff.dll model: it's built standalone and overrides
    the stock Qt-shipped qjpeg.dll, so it needs its own Deploy step (both
    into the Qt SDK, for dev runs via Qt Creator, and into the project
    release tree). See root CMakeLists.txt for the build-time override that
    applies to normal qimgv-plus builds.

.PARAMETER FullClean
    Remove the entire qjpeg_jpeg build folder before building (otherwise
    only CMakeCache.txt is cleared).

.PARAMETER SkipDeploy
    Skip deploying qjpeg.dll (and the shared jpeg*.dll) into the Qt SDK.

.EXAMPLE
    .\build_qjpeg_jpeg.ps1
    .\build_qjpeg_jpeg.ps1 -FullClean
#>
param(
    [switch] $FullClean,
    [switch] $SkipDeploy
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

$JPEG_INSTALL    = Join-Path $FORMATS_DIR "libjpeg-turbo\install"
$JPEG_BIN_DIR    = Join-Path $JPEG_INSTALL "bin"
$QJPEG_BUILD_DIR = Join-Path $SCRIPT_DIR "qjpeg_jpeg\build"
$QJPEG_INSTALL   = Join-Path $SCRIPT_DIR "qjpeg_jpeg\install"

# ---------------------------------------------------------------------------
# Common compiler / linker hardening flags (match build_qtiff_jpeg.ps1 / rebuild-all.ps1)
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
        "-DCMAKE_STATIC_LINKER_FLAGS_RELEASE=",
        "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=ON"
    )
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Write-Header($name) {
    Write-Host ""
    Write-Host ("=" * 60) -ForegroundColor DarkCyan
    Write-Host "  $name" -ForegroundColor Cyan
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

# Same resolution strategy as build_qtiff_jpeg.ps1's Get-JpegDllPath --
# don't hardcode the SO-versioned DLL name (e.g. jpeg62.dll).
function Get-JpegDllPath {
    if (-not (Test-Path $JPEG_BIN_DIR)) {
        return $null
    }
    $dll = Get-ChildItem -Path $JPEG_BIN_DIR -Filter "jpeg*.dll" -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notmatch "^turbojpeg" } |
        Select-Object -First 1
    if ($dll) { return $dll.FullName }
    return $null
}

# ---------------------------------------------------------------------------
# Precondition: shared libjpeg-turbo must already be built
# (formats/libjpeg-turbo/install) -- see build_qtiff_jpeg.ps1, Step 2.
# ---------------------------------------------------------------------------
$jpegImportLib = Join-Path $JPEG_INSTALL "lib\jpeg.lib"
if (-not (Test-Path $jpegImportLib)) {
    throw "libjpeg-turbo import library not found at: $jpegImportLib -- " +
          "run '.\build_qtiff_jpeg.ps1 -Steps libjpeg-turbo' first"
}
if (-not (Get-JpegDllPath)) {
    throw "No shared jpeg*.dll found in $JPEG_BIN_DIR -- " +
          "run '.\build_qtiff_jpeg.ps1 -Steps libjpeg-turbo' first"
}

# ---------------------------------------------------------------------------
# Step: qjpeg.dll (Qt plugin)
# ---------------------------------------------------------------------------
function Build-Qjpeg {
    $srcDir = Join-Path $SCRIPT_DIR "qjpeg_jpeg"

    # PREFIX_PATH must include Qt and libjpeg-turbo so find_package(Qt6)
    # and find_package(JPEG) both succeed.
    $prefixPath = "$QT_DIR;$JPEG_INSTALL"

    Clear-BuildDir $QJPEG_BUILD_DIR

    Write-Info "Configuring qjpeg plugin..."
    $args = @(
        "-S", $srcDir,
        "-B", $QJPEG_BUILD_DIR,
        "-A", "x64",
        "-DCMAKE_INSTALL_PREFIX=$QJPEG_INSTALL",
        "-DCMAKE_PREFIX_PATH=$prefixPath",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"
    ) + (Get-HardeningArgs)
    Invoke-CMake $args

    Write-Info "Building qjpeg.dll..."
    Invoke-CMake @("--build", $QJPEG_BUILD_DIR, "--config", "Release", "--parallel")

    Write-Info "Installing qjpeg.dll..."
    Invoke-CMake @("--install", $QJPEG_BUILD_DIR, "--config", "Release")

    Write-OK "qjpeg.dll installed to $QJPEG_INSTALL"
}

# ---------------------------------------------------------------------------
# Deploy: replace stock qjpeg.dll in Qt SDK (with backup), plus the shared
# jpeg*.dll runtime dependency it now needs.
# ---------------------------------------------------------------------------
function Deploy-Qjpeg {
    $customDll = Join-Path $QJPEG_INSTALL "qjpeg.dll"
    $stockDll  = Join-Path $QT_SDK_IMAGEFORMATS "qjpeg.dll"
    $backupDll = Join-Path $QT_SDK_IMAGEFORMATS "qjpeg.dll.stock"

    if (-not (Test-Path $customDll)) {
        Write-Warn "Custom qjpeg.dll not found at $customDll -- skipping deploy"
        return
    }

    # Back up original only if not already backed up
    if ((Test-Path $stockDll) -and -not (Test-Path $backupDll)) {
        Copy-Item $stockDll $backupDll -Force
        Write-Info "Backed up stock qjpeg.dll -> qjpeg.dll.stock"
    }

    Copy-Item $customDll $stockDll -Force
    Write-OK "Deployed custom qjpeg.dll to Qt SDK: $stockDll"

    # Also copy to project release directory if it exists
    $releaseDll = Join-Path $ROOT "release\imageformats\qjpeg.dll"
    $releaseDir = Split-Path $releaseDll -Parent
    if (Test-Path $releaseDir) {
        Copy-Item $customDll $releaseDll -Force
        Write-OK "Deployed custom qjpeg.dll to release: $releaseDll"
    }

    # qjpeg.dll now depends on the shared jpeg*.dll at load time. Windows'
    # default DLL search order does not include a plugin's own directory,
    # so it needs to land next to whatever loads the plugin (see
    # build_qtiff_jpeg.ps1's Deploy-JpegRuntime for the same reasoning).
    $jpegDll = Get-JpegDllPath
    if ($jpegDll) {
        $jpegDllName = Split-Path $jpegDll -Leaf

        $qtBinDll = Join-Path $QT_DIR "bin\$jpegDllName"
        Copy-Item $jpegDll $qtBinDll -Force
        Write-OK "Deployed $jpegDllName to Qt SDK bin: $qtBinDll"

        $releaseJpegDll = Join-Path $ROOT "release\$jpegDllName"
        if (Test-Path (Split-Path $releaseJpegDll -Parent)) {
            Copy-Item $jpegDll $releaseJpegDll -Force
            Write-OK "Deployed $jpegDllName to release: $releaseJpegDll"
        }
    } else {
        Write-Warn "No shared jpeg*.dll found -- qjpeg.dll will fail to load until it's deployed"
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

$sw = [System.Diagnostics.Stopwatch]::StartNew()

Write-Host ""
Write-Host "  qjpeg.dll (shared libjpeg-turbo) build" -ForegroundColor White
Write-Host "  Root : $ROOT"                           -ForegroundColor DarkGray
Write-Host "  jpeg : $JPEG_INSTALL"                   -ForegroundColor DarkGray
Write-Host ""

Write-Header "qjpeg"
try {
    Build-Qjpeg
} catch {
    $sw.Stop()
    Write-Host "  [FAIL]  $_" -ForegroundColor Red
    exit 1
}

if (-not $SkipDeploy) {
    Write-Header "Deploy"
    try {
        Deploy-Qjpeg
    } catch {
        $sw.Stop()
        Write-Host "  [FAIL]  Deploy: $_" -ForegroundColor Red
        exit 1
    }
}

$sw.Stop()
$elapsed = "{0:mm\:ss}" -f $sw.Elapsed

Write-Host ""
Write-Host ("=" * 60) -ForegroundColor DarkCyan
Write-Host "  Done  ($elapsed)" -ForegroundColor Cyan
Write-Host ("=" * 60) -ForegroundColor DarkCyan
Write-Host ""
