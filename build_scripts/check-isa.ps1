<#
.SYNOPSIS
    Verifies that release binaries conform to the target instruction set (SSE2 or AVX2).

.DESCRIPTION
    Runs `dumpbin /disasm` on every executable and DLL in the specified release directory,
    detecting VEX-encoded instructions (v-prefixed), ymm/zmm register usage, and
    vector memory operands.

    For SSE2 releases:
    - Binaries without AVX instructions pass with [OK].
    - Known runtime-dispatched libraries (ncnn in upscayl-ncnn, libjpeg-turbo, zlib-ng,
      libdeflate, zstd, FFmpeg/dav1d/aom/Highway in plugins, Qt SDK, and Microsoft
      redistributables) are allowlisted because their AVX routines are only reached
      after a runtime CPUID check.
    - Any unexpected first-party binary (e.g. qimgv-plus.exe, qimgvshellex.dll, kimg_hdr.dll,
      or custom qjpeg/qtiff) containing AVX/VEX instructions fails verification.

    Exits with code 1 if any unexpected AVX instructions are detected, or 0 on success.

.PARAMETER Path
    Path to the release directory (e.g. release-sse2 or release) or to a specific binary.
    If omitted, resolves to release-sse2 (for -Isa SSE2) or release (for -Isa AVX2).

.PARAMETER Isa
    Target instruction set: SSE2 (default for release-sse2) or AVX2 (for release).
    If omitted, inferred from the directory name.

.PARAMETER Strict
    Disables the runtime dispatch allowlist and fails on any AVX instruction in any binary.

.PARAMETER FullCount
    Disassembles each binary completely to count all AVX occurrences, rather than
    terminating early on the first detected match for maximum speed.

.PARAMETER ObjDir
    Path to the CMake build directory for the SSE2 target (e.g.
    out/build/qimgv-x64-release-sse2). When provided, the script scans all first-party
    .obj files for AVX/VEX instructions before scanning binaries. If every .obj is clean,
    any AVX found in non-allowlisted binaries is attributed to the MSVC CRT's
    vector_algorithms.obj (runtime-dispatched via __isa_available) and reported as
    [CRT-DISP] instead of [FAIL].

.PARAMETER Sde
    Launches the Intel Software Development Emulator (sde.exe) smoke test on qimgv-plus.exe
    under the pre-AVX CPU model: `sde -mrm -- <path>\qimgv-plus.exe`.

.PARAMETER SdePath
    Path to the sde executable (default: "sde").

.EXAMPLE
    .\build_scripts\check-isa.ps1 -Path release-sse2
    .\build_scripts\check-isa.ps1 -Path release -Isa AVX2
    .\build_scripts\check-isa.ps1 -Path release-sse2\qimgv-plus.exe
    .\build_scripts\check-isa.ps1 -Path release-sse2 -Sde
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string] $Path,

    [ValidateSet("SSE2", "AVX2")]
    [string] $Isa,

    [switch] $Strict,

    [switch] $FullCount,

    [string] $ObjDir,

    [switch] $Sde,

    [string] $SdePath = "sde"
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# VC developer environment & dumpbin bootstrap
# ---------------------------------------------------------------------------

function Import-VcVars {
    Write-Host "Configuring Visual Studio developer environment for dumpbin..." -ForegroundColor DarkGray
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

    $envDump = cmd.exe /c "call `"$vcvars`" x64 && set"
    if ($LASTEXITCODE -ne 0) {
        throw "vcvarsall.bat x64 failed with exit code $LASTEXITCODE"
    }

    foreach ($line in $envDump) {
        if ($line -match "^(.*?)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
            Set-Item -Path "env:$($matches[1])" -Value $matches[2]
        }
    }
}

function Ensure-Dumpbin {
    # 1. Look directly in MSVC installation via vswhere (most reliable)
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsInstallPath = & $vswhere -latest -property installationPath
        if ($vsInstallPath) {
            $msvcTools = Join-Path $vsInstallPath "VC\Tools\MSVC"
            if (Test-Path $msvcTools) {
                $candidates = Get-ChildItem -Path $msvcTools -Filter "dumpbin.exe" -Recurse -ErrorAction SilentlyContinue |
                              Where-Object { $_.FullName -match "\\bin\\Hostx64\\x64\\dumpbin\.exe$" }
                if (-not $candidates) {
                    $candidates = Get-ChildItem -Path $msvcTools -Filter "dumpbin.exe" -Recurse -ErrorAction SilentlyContinue |
                                  Where-Object { $_.FullName -match "\\bin\\Host[^\\]+\\x64\\dumpbin\.exe$" }
                }
                if ($candidates) {
                    $best = ($candidates | Select-Object -First 1).FullName
                    return $best
                }
            }
        }
    }

    # 2. Check current PATH
    $dumpbinCmd = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($dumpbinCmd -and ($dumpbinCmd.Source -notmatch "\\Windows\\System32\\dumpbin\.exe$")) {
        return $dumpbinCmd.Source
    }

    # 3. Bootstrap vcvarsall and re-check
    Import-VcVars
    $resolved = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($resolved) {
        return $resolved.Source
    }

    throw "dumpbin.exe could not be located via Visual Studio or PATH."
}

# ---------------------------------------------------------------------------
# Allowlist definition
# ---------------------------------------------------------------------------

function Get-AllowlistReason {
    param(
        [string] $RelativePath,
        [string] $FileName
    )

    # 1. First-party libraries with CPUID runtime dispatch
    if ($FileName -eq "upscayl-ncnn.dll") {
        return "ncnn runtime CPU dispatch (NCNN_RUNTIME_CPU=ON)"
    }

    # 2. Third-party shared libraries with CPUID runtime dispatch
    if ($FileName -match "^jpeg\d*\.dll$") {
        return "libjpeg-turbo runtime SIMD dispatch"
    }
    if ($FileName -eq "zlib1.dll") {
        return "zlib-ng runtime SIMD dispatch (ZLIB_COMPAT=ON)"
    }
    if ($FileName -eq "deflate.dll") {
        return "libdeflate runtime SIMD dispatch"
    }
    if ($FileName -eq "zstd.dll") {
        return "zstd runtime SIMD dispatch"
    }

    # 3. Plugins statically linking codecs with runtime dispatch
    if ($FileName -eq "kimg_avif.dll") {
        return "statically links dav1d, aom, libyuv (runtime CPU dispatch)"
    }
    if ($FileName -eq "kimg_jxl.dll") {
        return "statically links Highway (runtime CPU dispatch)"
    }
    if ($FileName -in @("kimg_ff.dll", "kimg_heif.dll")) {
        return "statically links FFmpeg (x86asm runtime CPU dispatch)"
    }

    # 4. Microsoft runtime redistributables (shipped by windeployqt / VC redist, SSE2 baseline)
    if ($FileName -in @("d3dcompiler_47.dll", "dxcompiler.dll", "dxil.dll", "vcomp140.dll")) {
        return "Microsoft redistributable (SSE2 baseline with runtime dispatch)"
    }

    # 5. Qt SDK binaries (official build, SSE2 baseline with CPUID runtime dispatch)
    if ($FileName -like "Qt6*.dll") {
        return "Qt SDK library (SSE2 baseline with CPUID runtime dispatch)"
    }
    if ($RelativePath -match "^(platforms|styles|generic|iconengines|networkinformation|sqldrivers)\\" -or
        $FileName -in @("qgif.dll", "qicns.dll", "qico.dll", "qpdf.dll", "qsvg.dll", "qtga.dll", "qwbmp.dll", "qwebp.dll")) {
        return "Qt SDK plugin (SSE2 baseline with CPUID runtime dispatch)"
    }

    return $null
}

# ---------------------------------------------------------------------------
# First-party .obj scan (CRT dispatch verification)
# ---------------------------------------------------------------------------

function Invoke-ObjScan {
    param(
        [string] $DumpbinPath,
        [string] $ScanDir
    )

    # Collect all .obj files, excluding runtime-dispatch subtrees and CMake probes
    $objFiles = Get-ChildItem -Path $ScanDir -Filter "*.obj" -Recurse -File |
                Where-Object {
                    $_.FullName -notmatch "\\upscayl-ncnn\\" -and
                    $_.FullName -notmatch "\\glslang\\" -and
                    $_.FullName -notmatch "\\SPIRV\\" -and
                    $_.FullName -notmatch "\\CompilerId"
                } |
                Sort-Object FullName

    $dirtyFiles = @()

    foreach ($obj in $objFiles) {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $DumpbinPath
        $psi.WorkingDirectory = Split-Path $DumpbinPath -Parent
        $psi.Arguments = "/disasm `"$($obj.FullName)`""
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.CreateNoWindow = $true

        $proc = [System.Diagnostics.Process]::Start($psi)
        $foundAvx = $false
        $sample = $null
        $lineCount = 0

        while (-not $proc.StandardOutput.EndOfStream) {
            $line = $proc.StandardOutput.ReadLine()
            $lineCount++

            # Same instruction regex as Test-BinaryIsa
            if ($line -match "^\s*[0-9A-Fa-f]+:\s+[0-9A-Fa-f\s]{2,}\s+(?<mnemonic>[a-z0-9]+)\b(?<rest>.*)$") {
                $mnemonic = $Matches['mnemonic']
                $rest = $Matches['rest']

                if (($mnemonic -like "v*" -and $mnemonic -ne "verr" -and $mnemonic -ne "verw") -or
                    ($mnemonic -match "^[yz]mm") -or
                    ($rest -match "\b[yz]mm\d*\b|[yz]mmword") -or
                    ($mnemonic -in @('andn', 'bzhi', 'mulx', 'pdep', 'pext', 'rorx', 'sarx', 'shlx', 'shrx'))) {
                    $foundAvx = $true
                    $sample = $line.Trim()
                    try { $proc.Kill() } catch {}
                    break
                }
            }
        }

        $proc.WaitForExit()

        if ($lineCount -eq 0) {
            Write-Warning "dumpbin produced no output for: $($obj.FullName)"
        }

        if ($foundAvx) {
            $relPath = $obj.FullName.Substring($ScanDir.Length).TrimStart("\/")
            $dirtyFiles += [PSCustomObject]@{ Path = $relPath; Sample = $sample }
        }
    }

    return [PSCustomObject]@{
        Scanned    = $objFiles.Count
        DirtyFiles = $dirtyFiles
    }
}

# ---------------------------------------------------------------------------
# Binary analysis
# ---------------------------------------------------------------------------

function Test-BinaryIsa {
    param(
        [string] $DumpbinPath,
        [string] $FilePath,
        [string] $RelativePath,
        [switch] $CollectFullCount
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $DumpbinPath
    $psi.WorkingDirectory = Split-Path $DumpbinPath -Parent
    $psi.Arguments = "/disasm `"$FilePath`""
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.CreateNoWindow = $true

    $proc = [System.Diagnostics.Process]::Start($psi)

    $hasAvx = $false
    $matchCount = 0
    $firstSample = $null

    while (-not $proc.StandardOutput.EndOfStream) {
        $line = $proc.StandardOutput.ReadLine()

        # Instruction line in dumpbin format: <address>: <hex bytes>  <mnemonic>  <operands>
        if ($line -match "^\s*[0-9A-Fa-f]+:\s+[0-9A-Fa-f\s]{2,}\s+(?<mnemonic>[a-z0-9]+)\b(?<rest>.*)$") {
            $mnemonic = $Matches['mnemonic']
            $rest = $Matches['rest']

            # Match VEX/EVEX prefix (v-prefixed instructions except legacy verr/verw),
            # ymm/zmm register operands, ymmword/zmmword memory sizes, or BMI instructions.
            $isMatch = $false
            if (($mnemonic -like "v*" -and $mnemonic -ne "verr" -and $mnemonic -ne "verw") -or
                ($mnemonic -match "^[yz]mm") -or
                ($rest -match "\b[yz]mm\d*\b|[yz]mmword") -or
                ($mnemonic -in @('andn', 'bzhi', 'mulx', 'pdep', 'pext', 'rorx', 'sarx', 'shlx', 'shrx'))) {
                $isMatch = $true
            }

            if ($isMatch) {
                $hasAvx = $true
                $matchCount++
                if ($null -eq $firstSample) {
                    $firstSample = $line.Trim()
                }

                if (-not $CollectFullCount) {
                    try {
                        $proc.Kill()
                    } catch {
                        # Process might have already terminated
                    }
                    break
                }
            }
        }
    }

    $proc.WaitForExit()

    $fileName = Split-Path $FilePath -Leaf
    $allowlistReason = Get-AllowlistReason -RelativePath $RelativePath -FileName $fileName

    return [PSCustomObject]@{
        FilePath        = $FilePath
        RelativePath    = $RelativePath
        FileName        = $fileName
        HasAvx          = $hasAvx
        MatchCount      = $matchCount
        FirstSample     = $firstSample
        IsAllowlisted   = ($null -ne $allowlistReason)
        AllowlistReason = $allowlistReason
    }
}

# ---------------------------------------------------------------------------
# Path & ISA resolution
# ---------------------------------------------------------------------------

$projectRoot = Split-Path $PSScriptRoot -Parent

if (-not $Path) {
    if ($Isa -eq "AVX2") {
        $Path = Join-Path $projectRoot "release"
    } elseif ($Isa -eq "SSE2") {
        $Path = Join-Path $projectRoot "release-sse2"
    } else {
        $sse2Candidate = Join-Path $projectRoot "release-sse2"
        $avx2Candidate = Join-Path $projectRoot "release"
        if (Test-Path $sse2Candidate) {
            $Path = $sse2Candidate
            $Isa = "SSE2"
        } elseif (Test-Path $avx2Candidate) {
            $Path = $avx2Candidate
            $Isa = "AVX2"
        } else {
            throw "Neither 'release-sse2' nor 'release' directory found under $projectRoot. Please specify -Path."
        }
    }
}

if (-not (Test-Path $Path)) {
    throw "Target path does not exist: $Path"
}

$resolvedPath = (Resolve-Path $Path).Path

if (-not $Isa) {
    if ($resolvedPath -match "[-_]sse2\b" -or $resolvedPath -match "\\release-sse2(\\|$)") {
        $Isa = "SSE2"
    } elseif ($resolvedPath -match "[-_]avx2\b" -or $resolvedPath -match "\\release(\\|$)") {
        $Isa = "AVX2"
    } else {
        $Isa = "SSE2"
    }
}

# Auto-resolve ObjDir from the conventional CMake preset build directory
if (-not $ObjDir) {
    $presetName = if ($Isa -eq "SSE2") { "qimgv-x64-release-sse2" } else { "qimgv-x64-release" }
    $candidate = Join-Path $projectRoot "out\build\$presetName"
    if (Test-Path $candidate) {
        $ObjDir = $candidate
    }
}

$dumpbinExe = Ensure-Dumpbin

# ---------------------------------------------------------------------------
# Target collection
# ---------------------------------------------------------------------------

$targets = @()
$isSingleFile = -not (Test-Path -Path $resolvedPath -PathType Container)

if ($isSingleFile) {
    $targets += (Get-Item $resolvedPath)
    $baseDir = Split-Path $resolvedPath -Parent
} else {
    $baseDir = $resolvedPath
    $targets = Get-ChildItem -Path $resolvedPath -Include *.exe,*.dll -Recurse -File |
               Sort-Object FullName
}

if ($targets.Count -eq 0) {
    Write-Host "No .exe or .dll files found in: $resolvedPath" -ForegroundColor Yellow
    exit 0
}

Write-Host ""
Write-Host ("=" * 70) -ForegroundColor DarkCyan
Write-Host "  ISA Verification ($Isa)" -ForegroundColor Cyan
Write-Host "  Target path: $resolvedPath" -ForegroundColor Gray
Write-Host "  Total files: $($targets.Count)" -ForegroundColor Gray
if ($Strict) {
    Write-Host "  Mode: STRICT (runtime dispatch allowlist disabled)" -ForegroundColor Yellow
}
Write-Host ("=" * 70) -ForegroundColor DarkCyan
Write-Host ""

# ---------------------------------------------------------------------------
# First-party .obj verification (opt-in via -ObjDir)
# ---------------------------------------------------------------------------

$crtDispatchVerified = $false

if ($ObjDir -and $Isa -eq "SSE2") {
    $resolvedObjDir = (Resolve-Path $ObjDir -ErrorAction SilentlyContinue)
    if (-not $resolvedObjDir) {
        Write-Host "WARNING: -ObjDir path not found: $ObjDir" -ForegroundColor Yellow
        Write-Host "         CRT dispatch verification skipped." -ForegroundColor Yellow
    } else {
        $resolvedObjDir = $resolvedObjDir.Path
        Write-Host "  First-party .obj scan: $resolvedObjDir" -ForegroundColor Gray
        $objResult = Invoke-ObjScan -DumpbinPath $dumpbinExe -ScanDir $resolvedObjDir
        if ($objResult.DirtyFiles.Count -gt 0) {
            Write-Host "  WARNING: $($objResult.DirtyFiles.Count)/$($objResult.Scanned) .obj file(s) contain AVX:" -ForegroundColor Red
            foreach ($df in $objResult.DirtyFiles) {
                Write-Host "    $($df.Path)" -ForegroundColor Red
                Write-Host "      $($df.Sample)" -ForegroundColor DarkRed
            }
            Write-Host "  CRT dispatch verification FAILED. Binary [FAIL] results are genuine." -ForegroundColor Red
        } else {
            Write-Host "  Result: $($objResult.Scanned) .obj files scanned, all SSE2-clean." -ForegroundColor Green
            Write-Host "  AVX in non-allowlisted binaries is MSVC CRT runtime dispatch." -ForegroundColor Green
            $crtDispatchVerified = $true
        }
        Write-Host ""
    }
}

# ---------------------------------------------------------------------------
# Scanning
# ---------------------------------------------------------------------------

$passedCount      = 0
$allowlistCount   = 0
$crtDispatchCount = 0
$unexpectedCount  = 0
$failedBinaries   = @()

foreach ($target in $targets) {
    $relPath = if ($target.FullName.StartsWith($baseDir, [System.StringComparison]::OrdinalIgnoreCase)) {
        $target.FullName.Substring($baseDir.Length).TrimStart("\/")
    } else {
        $target.Name
    }

    $result = Test-BinaryIsa -DumpbinPath $dumpbinExe `
                             -FilePath $target.FullName `
                             -RelativePath $relPath `
                             -CollectFullCount:$FullCount

    if ($Isa -eq "SSE2") {
        if (-not $result.HasAvx) {
            Write-Host "  [OK]         $relPath (SSE2 baseline)" -ForegroundColor Green
            $passedCount++
        } elseif ($result.IsAllowlisted -and (-not $Strict)) {
            Write-Host "  [ALLOWLIST]  $relPath ($($result.AllowlistReason))" -ForegroundColor Cyan
            $allowlistCount++
        } elseif ($crtDispatchVerified -and (-not $Strict)) {
            Write-Host "  [CRT-DISP]   $relPath (MSVC CRT vector_algorithms runtime dispatch)" -ForegroundColor DarkCyan
            $crtDispatchCount++
        } else {
            Write-Host "  [FAIL]       $relPath" -ForegroundColor Red
            if ($result.FirstSample) {
                Write-Host "               Sample: $($result.FirstSample)" -ForegroundColor DarkRed
            }
            if ($FullCount -and $result.MatchCount -gt 0) {
                Write-Host "               Total AVX/VEX occurrences: $($result.MatchCount)" -ForegroundColor DarkRed
            }
            $unexpectedCount++
            $failedBinaries += $result
        }
    } else {
        # AVX2 target verification
        if ($result.HasAvx) {
            Write-Host "  [OK]         $relPath (AVX2/VEX present)" -ForegroundColor Green
            $passedCount++
        } else {
            # In an AVX2 build, first-party binaries should have AVX2
            $isFirstParty = ($result.FileName -in @("qimgv-plus.exe", "qimgvshellex.dll", "kimg_hdr.dll"))
            if ($isFirstParty) {
                Write-Host "  [WARN]       $relPath (No AVX/VEX detected, expected AVX2)" -ForegroundColor Yellow
            } else {
                Write-Host "  [INFO]       $relPath (SSE2 baseline)" -ForegroundColor DarkGray
            }
            $passedCount++
        }
    }
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host ("-" * 70) -ForegroundColor DarkCyan
Write-Host "Summary: $($targets.Count) total scanned" -ForegroundColor Cyan
if ($Isa -eq "SSE2") {
    Write-Host "  SSE2-clean:          $passedCount" -ForegroundColor Green
    Write-Host "  Allowlisted runtime: $allowlistCount" -ForegroundColor Cyan
    if ($crtDispatchCount -gt 0) {
        Write-Host "  CRT dispatch (ok):   $crtDispatchCount" -ForegroundColor DarkCyan
    }
    if ($unexpectedCount -gt 0) {
        Write-Host "  Unexpected AVX:      $unexpectedCount" -ForegroundColor Red
    }
} else {
    Write-Host "  Scanned binaries:    $passedCount" -ForegroundColor Green
}
Write-Host ("-" * 70) -ForegroundColor DarkCyan

if ($unexpectedCount -gt 0) {
    Write-Host ""
    Write-Host "VERIFICATION FAILED: $unexpectedCount binary/binaries contain unexpected AVX/VEX instructions." -ForegroundColor Red
    exit 1
} else {
    Write-Host ""
    Write-Host "VERIFICATION PASSED for $Isa target." -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# Optional SDE Smoke Test
# ---------------------------------------------------------------------------

if ($Sde) {
    Write-Host ""
    Write-Host ("=" * 70) -ForegroundColor DarkCyan
    Write-Host "  Intel SDE Smoke Test" -ForegroundColor Cyan
    Write-Host ("=" * 70) -ForegroundColor DarkCyan

    $sdeExe = Get-Command $SdePath -ErrorAction SilentlyContinue
    $exePath = if ($isSingleFile) {
        $resolvedPath
    } else {
        Join-Path $resolvedPath "qimgv-plus.exe"
    }

    if (-not (Test-Path $exePath)) {
        Write-Host "Executable not found for SDE smoke test: $exePath" -ForegroundColor Red
        exit 1
    }

    if (-not $sdeExe) {
        Write-Host "Intel Software Development Emulator ($SdePath) not found on PATH." -ForegroundColor Yellow
        Write-Host "To run manually:" -ForegroundColor Gray
        Write-Host "  $SdePath -mrm -- `"$exePath`"" -ForegroundColor Cyan
    } else {
        Write-Host "Running: $($sdeExe.Source) -mrm -- `"$exePath`"" -ForegroundColor Cyan
        & $sdeExe.Source -mrm -- $exePath
        if ($LASTEXITCODE -ne 0) {
            Write-Host "SDE exited with non-zero code $LASTEXITCODE" -ForegroundColor Red
            exit $LASTEXITCODE
        }
        Write-Host "SDE smoke test completed successfully." -ForegroundColor Green
    }
}

exit 0
