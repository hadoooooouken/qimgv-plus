<#
.SYNOPSIS
    Clone all third-party dependencies and apply local patches.

.DESCRIPTION
    This script automates the setup of the formats/ directory for building
    qimgv-plus from source. It:
      1. Clones each dependency at the exact required tag/commit
      2. Applies local patch files from the patches/ directory
      3. Prints download instructions for tools and sources that are not
         cloned (NASM, Ninja, LZMA SDK, UnRAR)

    Run this once after cloning the main qimgv-plus repository.

.PARAMETER SkipClone
    Skip cloning repos that already exist (useful for re-applying patches).

.PARAMETER PatchOnly
    Only apply patches, skip all cloning and downloading.

.EXAMPLE
    .\setup-deps.ps1
    .\setup-deps.ps1 -SkipClone
    .\setup-deps.ps1 -PatchOnly
#>
param(
    [switch] $SkipClone,
    [switch] $PatchOnly
)

$ErrorActionPreference = "Stop"

$ROOT       = (Resolve-Path "$PSScriptRoot\..").Path
$FORMATS    = Join-Path $ROOT "formats"
$PATCHES    = Join-Path $ROOT "patches"

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

function Clone-Repo {
    param(
        [string] $Url,
        [string] $Tag,
        [string] $DestDir,
        [switch] $Recursive
    )

    if (Test-Path $DestDir) {
        if ($SkipClone) {
            Write-Info "Already exists, skipping: $DestDir"
            return
        }
        Write-Warn "Directory exists, removing: $DestDir"
        Remove-Item $DestDir -Recurse -Force
    }

    $cloneArgs = @("clone", "--depth", "1", "--branch", $Tag, $Url, $DestDir)
    if ($Recursive) {
        $cloneArgs = @("clone", "--depth", "1", "--branch", $Tag, "--recurse-submodules", "--shallow-submodules", $Url, $DestDir)
    }

    Write-Info "git $($cloneArgs -join ' ')"
    & git @cloneArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to clone $Url at tag $Tag"
    }
    Write-OK "Cloned $Url @ $Tag"
}

function Apply-Patch {
    param(
        [string] $RepoDir,
        [string] $PatchFile
    )

    if (-not (Test-Path $PatchFile)) {
        Write-Warn "Patch file not found: $PatchFile"
        return
    }

    Write-Info "Applying patch: $(Split-Path $PatchFile -Leaf)"
    git -C $RepoDir apply --whitespace=nowarn $PatchFile
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to apply patch: $PatchFile"
    }
    Write-OK "Patch applied successfully"
}

function Apply-PatchSeries {
    param(
        [string] $RepoDir,
        [string] $PatchDir
    )

    if (-not (Test-Path $PatchDir)) {
        Write-Warn "Patch directory not found: $PatchDir"
        return
    }

    $patches = Get-ChildItem $PatchDir -Filter "*.patch" | Sort-Object Name
    if ($patches.Count -eq 0) {
        Write-Info "No patches found in $PatchDir"
        return
    }

    Write-Info "Applying $($patches.Count) patches from $(Split-Path $PatchDir -Leaf)/"
    foreach ($p in $patches) {
        git -C $RepoDir am --whitespace=nowarn $p.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to apply patch: $($p.Name)"
        }
    }
    Write-OK "$($patches.Count) patches applied"
}

# ---------------------------------------------------------------------------
# Dependency definitions
# ---------------------------------------------------------------------------

# Git-cloned dependencies with their upstream URLs and tags
$GIT_DEPS = @(
    @{ Name = "Imath";          Url = "https://github.com/AcademySoftwareFoundation/Imath.git";   Tag = "v3.2.3"       }
    @{ Name = "openexr";        Url = "https://github.com/AcademySoftwareFoundation/openexr.git"; Tag = "v3.5.2"       }
    @{ Name = "libavif";        Url = "https://github.com/AOMediaCodec/libavif.git";              Tag = "v1.4.2";     Recursive = $true }
    @{ Name = "libjxl";         Url = "https://github.com/libjxl/libjxl.git";                     Tag = "v0.12.0";    Recursive = $true }
    @{ Name = "jxrlib";         Url = "https://github.com/4creators/jxrlib.git";                   Tag = "v2019.10.9"   }
    @{ Name = "LibRaw";         Url = "https://github.com/LibRaw/LibRaw.git";                      Tag = "0.22.2"       }
    @{ Name = "ffmpeg";         Url = "https://git.ffmpeg.org/ffmpeg.git";                         Tag = "n9.0.2"       }
    # This pinned baseline is required by the local patch series below.
    @{ Name = "kimageformats";  Url = "https://invent.kde.org/frameworks/kimageformats.git";       Tag = "v6.26.0"      }
    @{ Name = "openjpeg";       Url = "https://github.com/uclouvain/openjpeg.git";                 Tag = "v2.5.4"       }
    @{ Name = "OpenJPH";        Url = "https://github.com/aous72/OpenJPH.git";                     Tag = "0.32.0"       }
    @{ Name = "libdeflate";     Url = "https://github.com/ebiggers/libdeflate.git";                Tag = "v1.26"        }
    @{ Name = "zlib-ng";        Url = "https://github.com/zlib-ng/zlib-ng.git";                    Tag = "2.3.3"        }
    @{ Name = "libjpeg-turbo";  Url = "https://github.com/libjpeg-turbo/libjpeg-turbo.git";        Tag = "3.2.0"        }
    @{ Name = "libspng";        Url = "https://github.com/randy408/libspng.git";                   Tag = "v0.7.4"       }
    @{ Name = "libtiff";        Url = "https://gitlab.com/libtiff/libtiff.git";                    Tag = "v4.7.2"       }
    @{ Name = "zstd";           Url = "https://github.com/facebook/zstd.git";                      Tag = "v1.5.7"       }
    @{ Name = "exiv2";          Url = "https://github.com/Exiv2/exiv2.git";                        Tag = "v0.28.9"      }
)

# Single-file patches (applied with git apply)
$SINGLE_PATCHES = @(
    @{ Dep = "jxrlib";     Patch = "jxrlib-v2019.10.9-uintptr-fix.patch"   }
    @{ Dep = "jxrlib";     Patch = "jxrlib-v2019.10.9-cmake-build.patch"   }
    @{ Dep = "LibRaw";     Patch = "LibRaw-0.22.2-vcxproj-toolset.patch"   }
)

# Multi-commit patch series (applied with git am)
$PATCH_SERIES = @(
    @{ Dep = "kimageformats"; PatchDir = "kimageformats" }
    @{ Dep = "openjpeg";      PatchDir = "openjpeg"      }
)

# Prebuilt downloads
$NASM_URL   ="https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip"

# ---------------------------------------------------------------------------
# Phase 1: Clone dependencies
# ---------------------------------------------------------------------------

if (-not $PatchOnly) {
    Write-Host ""
    Write-Host "  qimgv-plus dependency setup" -ForegroundColor White
    Write-Host "  Root    : $ROOT"             -ForegroundColor DarkGray
    Write-Host "  Formats : $FORMATS"          -ForegroundColor DarkGray
    Write-Host ""

    if (-not (Test-Path $FORMATS)) {
        New-Item -ItemType Directory -Path $FORMATS -Force | Out-Null
    }

    foreach ($dep in $GIT_DEPS) {
        Write-Header $dep.Name
        $destDir = Join-Path $FORMATS $dep.Name
        try {
            $recursive = if ($dep.Recursive) { $true } else { $false }
            if ($recursive) {
                Clone-Repo -Url $dep.Url -Tag $dep.Tag -DestDir $destDir -Recursive
            } else {
                Clone-Repo -Url $dep.Url -Tag $dep.Tag -DestDir $destDir
            }
        } catch {
            Write-Host "  [FAIL] $_" -ForegroundColor Red
        }
    }

    # magic-kernel-sharp is a data file, not a library -- instructions only
    $mksDir = Join-Path $FORMATS "magic-kernel-sharp"
    if (-not (Test-Path $mksDir)) {
        Write-Header "magic-kernel-sharp"
        Write-Info "Creating placeholder directory. See BUILDING.md for details."
        New-Item -ItemType Directory -Path $mksDir -Force | Out-Null
    }

    # NASM binary
    Write-Header "nasm (tool)"
    $nasmDir = Join-Path $FORMATS "nasm"
    if (Test-Path (Join-Path $nasmDir "nasm.exe")) {
        Write-Info "Already exists, skipping NASM download"
    } else {
        Write-Info "Download NASM from: https://www.nasm.us/"
        Write-Info "Place nasm.exe into: $nasmDir"
        Write-Warn "Automatic download not implemented -- please download manually"
    }

    # Ninja binary
    Write-Header "ninja (tool)"
    $ninjaDir = Join-Path $FORMATS "ninja"
    if (Test-Path (Join-Path $ninjaDir "ninja.exe")) {
        Write-Info "Already exists, skipping Ninja download"
    } else {
        Write-Info "Download Ninja from: https://github.com/ninja-build/ninja/releases"
        Write-Info "Place ninja.exe into: $ninjaDir"
        Write-Warn "Automatic download not implemented -- please download manually"
    }

    # LZMA SDK
    Write-Header "lzma (LZMA SDK)"
    $lzmaDir = Join-Path $FORMATS "lzma"
    if (Test-Path (Join-Path $lzmaDir "C\7z.h")) {
        Write-Info "Already exists, skipping LZMA SDK setup"
    } else {
        Write-Info "Download LZMA SDK from: https://www.7-zip.org/sdk.html"
        Write-Info "Extract to: $lzmaDir (must contain C\ directory)"
        Write-Warn "Automatic download not implemented -- please extract LZMA SDK manually"
    }

    # UnRAR source
    Write-Header "unrar (RAR source)"
    $unrarDir = Join-Path $FORMATS "unrar"
    if (Test-Path (Join-Path $unrarDir "version.hpp")) {
        Write-Info "Already exists, skipping UnRAR source setup"
    } else {
        Write-Info "Download the latest UnRAR source from: https://www.rarlab.com/rar_add.htm"
        Write-Info "Extract source files directly into: $unrarDir"
        Write-Warn "Automatic download not implemented -- please extract UnRAR source manually"
    }
}

# ---------------------------------------------------------------------------
# Phase 2: Apply patches
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "  Applying patches" -ForegroundColor White
Write-Host ""

foreach ($p in $SINGLE_PATCHES) {
    Write-Header "$($p.Dep) (patch)"
    $repoDir   = Join-Path $FORMATS $p.Dep
    $patchFile = Join-Path $PATCHES $p.Patch
    try {
        Apply-Patch -RepoDir $repoDir -PatchFile $patchFile
    } catch {
        Write-Host "  [FAIL] $_" -ForegroundColor Red
    }
}

foreach ($ps in $PATCH_SERIES) {
    Write-Header "$($ps.Dep) (patch series)"
    $repoDir  = Join-Path $FORMATS $ps.Dep
    $patchDir = Join-Path $PATCHES $ps.PatchDir
    try {
        Apply-PatchSeries -RepoDir $repoDir -PatchDir $patchDir
    } catch {
        Write-Host "  [FAIL] $_" -ForegroundColor Red
    }
}

# ---------------------------------------------------------------------------
# Phase 3: Clone upscayl-ncnn
# ---------------------------------------------------------------------------

if (-not $PatchOnly) {
    Write-Header "upscayl-ncnn"
    $upscaylDir = Join-Path $ROOT "upscayl-ncnn"
    if (Test-Path $upscaylDir) {
        Write-Info "Already exists, skipping upscayl-ncnn"
    } else {
        try {
            Clone-Repo `
                -Url "https://github.com/hadoooooouken/upscayl-ncnn-qimgv-plus.git" `
                -Tag "qimgv" `
                -DestDir $upscaylDir `
                -Recursive
        } catch {
            Write-Host "  [FAIL] $_" -ForegroundColor Red
        }
    }
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host ("=" * 60) -ForegroundColor DarkCyan
Write-Host "  Setup complete" -ForegroundColor Cyan
Write-Host ("=" * 60) -ForegroundColor DarkCyan
Write-Host ""
Write-Host "  Next steps:" -ForegroundColor White
Write-Host "    1. Download NASM if not already present (see above)" -ForegroundColor Gray
Write-Host "    2. Run build_scripts\build_qtiff_jpeg.ps1" -ForegroundColor Gray
Write-Host "    3. Run build_scripts\build_qpng_spng.ps1" -ForegroundColor Gray
Write-Host "    4. Run build_scripts\build_qjpeg_jpeg.ps1" -ForegroundColor Gray
Write-Host "    5. Run build_scripts\build_exiv2.ps1" -ForegroundColor Gray
Write-Host "    6. Run build_scripts\rebuild-all.ps1" -ForegroundColor Gray
Write-Host "    7. (Optional) Run build_scripts\build_ffmpeg_msvc.py" -ForegroundColor Gray
Write-Host "    8. Configure and build qimgv-plus (see BUILDING.md)" -ForegroundColor Gray
Write-Host ""
