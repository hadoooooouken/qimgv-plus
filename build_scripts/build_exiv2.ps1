<#
.SYNOPSIS
    Build exiv2 (static, minimal) for EXIF reading in qimgv-plus.

.DESCRIPTION
    Single-step build:
      1. exiv2 (static library, Release and Debug configurations)

    qimgv-plus only reads a handful of EXIF tags from memory-mapped files
    (DocumentInfo::loadExifTags), so the library is configured with the
    smallest feature set that keeps EXIF coverage for every image format
    the application opens:
      - XMP, video, web access, NLS, Brotli, inih and Nikon lens data are
        disabled (no expat, brotli or inih dependency).
      - PNG support stays enabled (zlib) for eXIf / raw-profile chunks.
      - BMFF support stays enabled for EXIF in HEIF, AVIF and JPEG XL.

    The library is linked statically into qimgv-plus.exe, so it is built
    with the shared MSVC policy (msvc-release-policy.json); /guard:ehcont
    on every object is required for the /guard:ehcont link of the exe.
    Release objects use IPO (/GL) like the other static libraries linked
    into LTCG targets.

    Both configurations are installed into one prefix (Debug with a "d"
    postfix) because a static C++ library must match the consumer's CRT
    and iterator debug level: the Debug preset links exiv2d.lib.

    Reuses the zlib-ng install already produced by build_qtiff_jpeg.ps1
    (formats/zlib-ng/install) -- it is not rebuilt here.

.PARAMETER Configs
    CMake configurations to build and install. Default = Release, Debug.

.PARAMETER FullClean
    Remove the entire exiv2 build folder before building (otherwise only
    CMakeCache.txt is cleared).

.EXAMPLE
    .\build_exiv2.ps1
    .\build_exiv2.ps1 -Configs Release
    .\build_exiv2.ps1 -FullClean
#>
param(
    [ValidateSet("Release", "Debug")]
    [string[]] $Configs = @("Release", "Debug"),
    [switch]   $FullClean
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
$ROOT        = (Resolve-Path "$PSScriptRoot\..").Path
$FORMATS_DIR = Join-Path $ROOT "formats"

$ZLIB_INSTALL    = Join-Path $FORMATS_DIR "zlib-ng\install"
$EXIV2_SRC_DIR   = Join-Path $FORMATS_DIR "exiv2"
$EXIV2_BUILD_DIR = Join-Path $EXIV2_SRC_DIR "build_msvc"
$EXIV2_INSTALL   = Join-Path $EXIV2_SRC_DIR "install"

# ---------------------------------------------------------------------------
# Common compiler / linker policy (single source: msvc-release-policy.json)
# ---------------------------------------------------------------------------
. (Join-Path $PSScriptRoot "MsvcPolicy.ps1")

function Get-HardeningArgs {
    return Get-MsvcPolicyCMakeArgs -Ipo
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
# Preconditions
# ---------------------------------------------------------------------------

# zlib-ng must already be built (shared by qtiff.dll, kimg_png and the root
# qimgv-plus target -- see the EXISTS check in qimgv/CMakeLists.txt).
$zlibLib = Join-Path $ZLIB_INSTALL "lib\zlib.lib"
if (-not (Test-Path $zlibLib)) {
    throw "zlib-ng import library not found at: $zlibLib -- run build_qtiff_jpeg.ps1 first"
}

# formats/exiv2 used to hold the official prebuilt bundle; the source tree
# is cloned there by setup-deps.ps1 now.
if (-not (Test-Path (Join-Path $EXIV2_SRC_DIR "CMakeLists.txt"))) {
    throw "exiv2 source not found at: $EXIV2_SRC_DIR -- remove any prebuilt bundle there and run setup-deps.ps1"
}

# ---------------------------------------------------------------------------
# Step 1: exiv2 (static)
# ---------------------------------------------------------------------------
function Build-Exiv2 {
    Clear-BuildDir $EXIV2_BUILD_DIR

    Write-Info "Configuring exiv2 (static, EXIF-only, zlib-ng, BMFF)..."
    $cmakeArgs = @(
        "-S", $EXIV2_SRC_DIR,
        "-B", $EXIV2_BUILD_DIR,
        "-A", "x64",
        "-DCMAKE_INSTALL_PREFIX=$EXIV2_INSTALL",
        "-DCMAKE_PREFIX_PATH=$ZLIB_INSTALL",
        "-DCMAKE_DEBUG_POSTFIX=d",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
        # exiv2 links Iconv when any is found; a stray one on PATH must not leak in.
        "-DCMAKE_DISABLE_FIND_PACKAGE_Iconv=ON",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DEXIV2_ENABLE_DYNAMIC_RUNTIME=ON",
        "-DEXIV2_ENABLE_XMP=OFF",
        "-DEXIV2_ENABLE_EXTERNAL_XMP=OFF",
        "-DEXIV2_ENABLE_PNG=ON",
        "-DEXIV2_ENABLE_BMFF=ON",
        "-DEXIV2_ENABLE_BROTLI=OFF",
        "-DEXIV2_ENABLE_VIDEO=OFF",
        "-DEXIV2_ENABLE_WEBREADY=OFF",
        "-DEXIV2_ENABLE_CURL=OFF",
        "-DEXIV2_ENABLE_NLS=OFF",
        "-DEXIV2_ENABLE_INIH=OFF",
        "-DEXIV2_ENABLE_LENSDATA=OFF",
        "-DEXIV2_BUILD_EXIV2_COMMAND=OFF",
        "-DEXIV2_BUILD_SAMPLES=OFF",
        "-DEXIV2_BUILD_UNIT_TESTS=OFF",
        "-DEXIV2_BUILD_FUZZ_TESTS=OFF",
        "-DEXIV2_BUILD_DOC=OFF"
    ) + (Get-HardeningArgs)
    Invoke-CMake $cmakeArgs

    foreach ($config in $Configs) {
        Write-Info "Building exiv2 ($config)..."
        Invoke-CMake @("--build", $EXIV2_BUILD_DIR, "--config", $config, "--parallel")

        Write-Info "Installing exiv2 ($config)..."
        Invoke-CMake @("--install", $EXIV2_BUILD_DIR, "--config", $config)
    }

    Write-OK "exiv2 ($($Configs -join ', ')) installed to $EXIV2_INSTALL"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

$sw = [System.Diagnostics.Stopwatch]::StartNew()

Write-Host ""
Write-Host "  exiv2 (static) build"              -ForegroundColor White
Write-Host "  Root    : $ROOT"                   -ForegroundColor DarkGray
Write-Host "  zlib    : $ZLIB_INSTALL"           -ForegroundColor DarkGray
Write-Host "  Configs : $($Configs -join ', ')"  -ForegroundColor DarkGray
Write-Host ""

Write-Header "exiv2"
try {
    Build-Exiv2
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
Write-Host "  Next: re-run CMake configure for qimgv-plus so it links"
Write-Host "  the static exiv2 from $EXIV2_INSTALL."
Write-Host ""
