<#
.SYNOPSIS
    Build libspng (static) for the kimg_png Qt image format plugin.

.DESCRIPTION
    Single-step build:
      1. libspng (static, SIMD unfilter intrinsics via ENABLE_OPT)

    Reuses the zlib-ng install already produced by build_qtiff_jpeg.ps1
    (formats/zlib-ng/install) -- it is not rebuilt here.

    The resulting spng_static.lib is picked up automatically by
    3rdparty/kimageformats/CMakeLists.txt on the next qimgv-plus configure,
    producing kimg_png.dll in imageformats/ next to the exe. No Deploy-*
    step is needed here (unlike qtiff.dll, kimg_png is a normal plugin
    target built as part of the main qimgv-plus build tree, and gets
    copied to imageformats/ by the existing deployment step in the root
    CMakeLists.txt).

.PARAMETER FullClean
    Remove the entire libspng build folder before building (otherwise only
    CMakeCache.txt is cleared).

.EXAMPLE
    .\build_qpng_spng.ps1
    .\build_qpng_spng.ps1 -FullClean
#>
param(
    [switch] $FullClean
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
$ROOT        = (Resolve-Path "$PSScriptRoot\..").Path
$FORMATS_DIR = Join-Path $ROOT "formats"

$ZLIB_INSTALL   = Join-Path $FORMATS_DIR "zlib-ng\install"
$SPNG_SRC_DIR   = Join-Path $FORMATS_DIR "libspng"
$SPNG_BUILD_DIR = Join-Path $SPNG_SRC_DIR "build_msvc"
$SPNG_INSTALL   = Join-Path $SPNG_SRC_DIR "install"

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
        "-DCMAKE_STATIC_LINKER_FLAGS_RELEASE="
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
# Precondition: zlib-ng must already be built (shared by qtiff.dll and by
# the root qimgv-plus target -- see the EXISTS check in root CMakeLists.txt).
# ---------------------------------------------------------------------------
$zlibLib = Join-Path $ZLIB_INSTALL "lib\zlib.lib"
if (-not (Test-Path $zlibLib)) {
    throw "zlib-ng import library not found at: $zlibLib -- run build_qtiff_jpeg.ps1 first"
}

# ---------------------------------------------------------------------------
# Step 1: libspng (static)
# ---------------------------------------------------------------------------
function Build-Spng {
    if (-not (Test-Path $SPNG_SRC_DIR)) {
        throw "libspng source not found at: $SPNG_SRC_DIR"
    }

    Clear-BuildDir $SPNG_BUILD_DIR

    Write-Info "Configuring libspng (static, zlib-ng, SIMD unfilter)..."
    $cmakeArgs = @(
        "-S", $SPNG_SRC_DIR,
        "-B", $SPNG_BUILD_DIR,
        "-A", "x64",
        "-DCMAKE_INSTALL_PREFIX=$SPNG_INSTALL",
        "-DCMAKE_PREFIX_PATH=$ZLIB_INSTALL",
        "-DSPNG_STATIC=ON",
        "-DSPNG_SHARED=OFF",
        "-DSPNG_USE_MINIZ=OFF",
        "-DENABLE_OPT=ON",
        "-DBUILD_EXAMPLES=OFF",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"
    ) + (Get-HardeningArgs)
    Invoke-CMake $cmakeArgs

    Write-Info "Building libspng..."
    Invoke-CMake @("--build", $SPNG_BUILD_DIR, "--config", "Release", "--parallel")

    Write-Info "Installing libspng..."
    Invoke-CMake @("--install", $SPNG_BUILD_DIR, "--config", "Release")

    Write-OK "libspng installed to $SPNG_INSTALL"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

$sw = [System.Diagnostics.Stopwatch]::StartNew()

Write-Host ""
Write-Host "  kimg_png (libspng) build" -ForegroundColor White
Write-Host "  Root : $ROOT"            -ForegroundColor DarkGray
Write-Host "  zlib : $ZLIB_INSTALL"    -ForegroundColor DarkGray
Write-Host ""

Write-Header "libspng"
try {
    Build-Spng
} catch {
    $sw.Stop()
    Write-Host "  [FAIL]  $_" -ForegroundColor Red
    exit 1
}

$sw.Stop()
$elapsed = "{0:mm\:ss}" -f $sw.Elapsed

Write-Host ""
Write-Host ("=" * 60) -ForegroundColor DarkCyan
Write-Host "  Done  ($elapsed)" -ForegroundColor Cyan
Write-Host ("=" * 60) -ForegroundColor DarkCyan
Write-Host "  Next: re-run CMake configure for qimgv-plus so"
Write-Host "  3rdparty/kimageformats picks up kimg_png automatically."
Write-Host ""