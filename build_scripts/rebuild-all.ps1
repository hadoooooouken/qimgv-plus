<#
.SYNOPSIS
    Rebuild all qimgv format libraries with AVX2 / LTCG / O2.

.PARAMETER Libraries
    Which libs to rebuild. Default = all.
    Example: .\rebuild-all.ps1 -Libraries libheif,libde265

.PARAMETER FullClean
    Remove entire build\ folder instead of just CMakeCache.txt.

.EXAMPLE
    .\rebuild-all.ps1
    .\rebuild-all.ps1 -Libraries libheif
    .\rebuild-all.ps1 -Libraries Imath,openexr -FullClean
#>
param(
    [string[]] $Libraries = @("Imath","openexr","libavif","libjxl","jxrlib","libdeflate","zstd","LibRaw"),
    [switch]   $FullClean
)

$ErrorActionPreference = "Stop"
$ROOT = (Resolve-Path "$PSScriptRoot\..\formats").Path

# ---------------------------------------------------------------------------
# VC dev environment bootstrap
# ---------------------------------------------------------------------------
# Some sub-builds (meson/ninja for dav1d, etc.) resolve "cl" via PATH instead
# of an absolute path. Without vcvarsall imported into THIS process, those
# nested build steps fail with "CreateProcess failed: The system cannot find
# the file specified." even though top-level MSBuild/cmake builds work fine.
# Importing vcvars here makes cl.exe visible to every child process spawned
# from this script (cmake -> MSBuild -> custom build steps -> meson -> ninja).

function Import-VcVars {
    Write-Info "Locating Visual Studio vcvarsall.bat..."
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at $vswhere"
    }

    $vsInstallPath = & $vswhere -latest -property installationPath
    if (-not $vsInstallPath) {
        throw "vswhere could not locate a Visual Studio installation."
    }

    $vcvars = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvars)) {
        throw "Could not locate vcvarsall.bat at $vcvars"
    }
    Write-Info "Using $vcvars"

    $envDump = cmd.exe /c "call `"$vcvars`" x64 && set"
    if ($LASTEXITCODE -ne 0) {
        throw "vcvarsall.bat x64 failed (exit code $LASTEXITCODE)"
    }

    foreach ($line in $envDump) {
        if ($line -match "^(.*?)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
        }
    }

    $cl = Get-Command cl -ErrorAction SilentlyContinue
    if (-not $cl) {
        throw "vcvarsall.bat ran but cl.exe is still not on PATH."
    }
    Write-Info "cl.exe resolved to $($cl.Source)"
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

function Clear-BuildCache($buildDir) {
    if (Test-Path $buildDir) {
        $cache = Join-Path $buildDir "CMakeCache.txt"
        $isNinja = $false
        if (Test-Path $cache) {
            $content = Get-Content $cache -Raw -ErrorAction SilentlyContinue
            if ($content -and $content -match "CMAKE_GENERATOR:INTERNAL=Ninja") {
                $isNinja = $true
            }
        }

        if ($FullClean -or $isNinja) {
            Remove-Item $buildDir -Recurse -Force -ErrorAction SilentlyContinue
            Write-Info "Full clean: removed $buildDir (reason: FullClean=$FullClean, wasNinja=$isNinja)"
        } else {
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

function Build-Library {
    param(
        [string]   $SrcDir,
        [string]   $BuildDir,
        [string[]] $ConfigArgs,
        [string]   $InstallDir = $null
    )

    Clear-BuildCache $BuildDir

    Write-Info "Configuring..."
    $hardeningArgs = @(
        "-DCMAKE_CXX_FLAGS_RELEASE=/MD /O2 /Ob2 /Oi /Ot /DNDEBUG /arch:AVX2 /GS /guard:cf /EHsc /Qspectre",
        "-DCMAKE_C_FLAGS_RELEASE=/MD /O2 /Ob2 /Oi /Ot /DNDEBUG /arch:AVX2 /GS /guard:cf /Qspectre",
        "-DCMAKE_SHARED_LINKER_FLAGS_RELEASE=/guard:cf /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /CETCOMPAT",
        "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=/guard:cf /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /CETCOMPAT"
    )
    $args = @("-S", $SrcDir, "-B", $BuildDir, "-A", "x64") + $hardeningArgs + $ConfigArgs
    Invoke-CMake $args

    Write-Info "Building..."
    Invoke-CMake @("--build", $BuildDir, "--config", "Release", "--parallel")

    if ($InstallDir) {
        Write-Info "Installing to $InstallDir..."
        Invoke-CMake @("--install", $BuildDir, "--config", "Release")
    }

    Write-OK "Done."
}

# ---------------------------------------------------------------------------
# Library definitions  (order = dependency order)
# ---------------------------------------------------------------------------

$ALL_LIBS = [ordered]@{

    "Imath" = {
        Build-Library `
            -SrcDir     "$ROOT\Imath" `
            -BuildDir   "$ROOT\Imath\build" `
            -InstallDir "$ROOT\Imath\install" `
            -ConfigArgs @(
                "-DCMAKE_INSTALL_PREFIX=$ROOT\Imath\install",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DBUILD_TESTING=OFF"
            )
    }

    "openexr" = {
        Build-Library `
            -SrcDir     "$ROOT\openexr" `
            -BuildDir   "$ROOT\openexr\build" `
            -InstallDir "$ROOT\openexr\install" `
            -ConfigArgs @(
                "-DCMAKE_INSTALL_PREFIX=$ROOT\openexr\install",
                "-DImath_DIR=$ROOT\Imath\install\lib\cmake\Imath",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DBUILD_TESTING=OFF",
                "-DOPENEXR_BUILD_TOOLS=OFF",
                "-DOPENEXR_INSTALL_TOOLS=OFF",
                "-DOPENEXR_BUILD_EXAMPLES=OFF",
                "-DOPENEXR_INSTALL_DOCS=OFF"
            )
    }

    "libavif" = {
        Build-Library `
            -SrcDir   "$ROOT\libavif" `
            -BuildDir "$ROOT\libavif\build" `
            -ConfigArgs @(
                "-DBUILD_SHARED_LIBS=OFF",
                "-DAVIF_CODEC_AOM=LOCAL",
                "-DAVIF_CODEC_AOM_DECODE=OFF",
                "-DAVIF_CODEC_AOM_ENCODE=ON",
                "-DAVIF_CODEC_DAV1D=LOCAL",
                "-DAVIF_LIBYUV=LOCAL",
                "-DAVIF_BUILD_APPS=OFF",
                "-DAVIF_BUILD_TESTS=OFF"
            )
    }

    "libjxl" = {
        Build-Library `
            -SrcDir   "$ROOT\libjxl" `
            -BuildDir "$ROOT\libjxl\build" `
            -ConfigArgs @(
                "-DBUILD_SHARED_LIBS=OFF",
                "-DBUILD_TESTING=OFF",
                "-DJPEGXL_BUNDLE_LIBPNG=OFF",
                "-DJPEGXL_ENABLE_TOOLS=OFF",
                "-DJPEGXL_ENABLE_BENCHMARK=OFF",
                "-DJPEGXL_ENABLE_DOXYGEN=OFF",
                "-DJPEGXL_ENABLE_MANPAGES=OFF",
                "-DJPEGXL_ENABLE_EXAMPLES=OFF",
                "-DJPEGXL_ENABLE_JNI=OFF",
                "-DJPEGXL_ENABLE_VIEWERS=OFF",
                "-DJPEGXL_ENABLE_PLUGINS=OFF"
            )
    }

    "jxrlib" = {
        Build-Library `
            -SrcDir     "$ROOT\jxrlib" `
            -BuildDir   "$ROOT\jxrlib\build" `
            -InstallDir "$ROOT\jxrlib\install" `
            -ConfigArgs @(
                "-DCMAKE_INSTALL_PREFIX=$ROOT\jxrlib\install",
                "-DBUILD_SHARED_LIBS=OFF"
            )
    }

    "libdeflate" = {
        Build-Library `
            -SrcDir     "$ROOT\libdeflate" `
            -BuildDir   "$ROOT\libdeflate\build" `
            -InstallDir "$ROOT\libdeflate\install" `
            -ConfigArgs @(
                "-DCMAKE_INSTALL_PREFIX=$ROOT\libdeflate\install",
                "-DLIBDEFLATE_BUILD_SHARED_LIB=ON",
                "-DLIBDEFLATE_BUILD_STATIC_LIB=ON",
                "-DLIBDEFLATE_BUILD_GZIP=OFF",
                "-DLIBDEFLATE_BUILD_TESTS=OFF"
            )
    }

    "zstd" = {
        Build-Library `
            -SrcDir     "$ROOT\zstd\build\cmake" `
            -BuildDir   "$ROOT\zstd\build-msvc" `
            -InstallDir "$ROOT\zstd\install" `
            -ConfigArgs @(
                "-DCMAKE_INSTALL_PREFIX=$ROOT\zstd\install",
                "-DZSTD_BUILD_STATIC=OFF",
                "-DZSTD_BUILD_SHARED=ON",
                "-DZSTD_BUILD_PROGRAMS=OFF",
                "-DZSTD_BUILD_TESTS=OFF",
                "-DZSTD_BUILD_CONTRIB=OFF",
                "-DZSTD_USE_STATIC_RUNTIME=OFF",
                "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=ON"
            )
    }

    "LibRaw" = {
        Write-Info "Locating Visual Studio vcvarsall.bat..."
        $vsInstallPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
        if (-not $vsInstallPath -or -not (Test-Path "$vsInstallPath\VC\Auxiliary\Build\vcvarsall.bat")) {
            throw "Could not locate vcvarsall.bat. Please ensure Visual Studio is installed."
        }
        $vcvars = "$vsInstallPath\VC\Auxiliary\Build\vcvarsall.bat"
        Write-Info "Using $vcvars"

        $librawDir = "$ROOT\LibRaw"
        $destDir = "$librawDir\buildfiles\release-x86_64"

        # Run nmake inside cmd with vcvarsall.bat environment loaded
        Write-Info "Cleaning LibRaw..."
        cmd.exe /c "call `"$vcvars`" x64 && cd /d `"$librawDir`" && nmake /f Makefile.msvc clean"
        if ($LASTEXITCODE -ne 0) {
            throw "LibRaw clean failed (exit code $LASTEXITCODE)"
        }

        Write-Info "Building LibRaw with AVX2, LTCG, OpenMP and hardening flags"
        cmd.exe /c "call `"$vcvars`" x64 && cd /d `"$librawDir`" && nmake /f Makefile.msvc COPT_OPT=`"/O2 /Ob2 /Oi /Ot /MD /DNDEBUG /arch:AVX2 /GL /openmp /GS /guard:cf /Qspectre`" CFLAGS=`"-DUSE_OPENMP`" LDFLAGS=`"/LTCG /guard:cf /NXCOMPAT /CETCOMPAT`""
        if ($LASTEXITCODE -ne 0) {
            throw "LibRaw build failed (exit code $LASTEXITCODE)"
        }

        # Copy the outputs to the expected buildfiles/release-x86_64 directory
        Write-Info "Deploying LibRaw binaries..."
        if (-not (Test-Path $destDir)) {
            New-Item -ItemType Directory -Path $destDir -Force | Out-Null
        }
        Copy-Item "$librawDir\bin\libraw.dll" "$destDir\" -Force
        Copy-Item "$librawDir\lib\libraw.lib" "$destDir\" -Force
        Copy-Item "$librawDir\lib\libraw.exp" "$destDir\" -Force
        Copy-Item "$librawDir\lib\libraw_static.lib" "$destDir\" -Force

        Write-OK "LibRaw compiled and deployed successfully."
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

$sw      = [System.Diagnostics.Stopwatch]::StartNew()
$failed  = @()
$built   = @()
$skipped = @()
$total   = $Libraries.Count
$n       = 0

Write-Host ""
Write-Host "  qimgv format libraries rebuild" -ForegroundColor White
Write-Host "  Root : $ROOT"         -ForegroundColor DarkGray
Write-Host "  Libs : $($Libraries -join ', ')" -ForegroundColor DarkGray
Write-Host "  Mode : $(if ($FullClean) { 'Full clean' } else { 'Cache-only clean' })" -ForegroundColor DarkGray

Import-VcVars

foreach ($lib in $Libraries) {
    $n++
    if (-not $ALL_LIBS.Contains($lib)) {
        Write-Header $n $total $lib
        Write-Warn "Unknown library '$lib' -- skipping."
        $skipped += $lib
        continue
    }

    Write-Header $n $total $lib
    try {
        & $ALL_LIBS[$lib]
        $built += $lib
    } catch {
        Write-Host "  [FAIL]  $_" -ForegroundColor Red
        $failed += $lib
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

if ($built)   { Write-Host "  Built  : $($built   -join ', ')" -ForegroundColor Green  }
if ($skipped) { Write-Host "  Skipped: $($skipped -join ', ')" -ForegroundColor Yellow }
if ($failed)  { Write-Host "  Failed : $($failed  -join ', ')" -ForegroundColor Red    }

if (-not $built.Contains("LibRaw")) {
    Write-Warn "LibRaw: no CMakeLists.txt -- build manually via Makefile.msvc if skipped"
}
Write-Host ""

if ($failed) { exit 1 }
