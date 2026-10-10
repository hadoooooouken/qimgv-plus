<#
.SYNOPSIS
    Shared MSVC Release policy for dependencies rebuilt by build_scripts.

.DESCRIPTION
    Dot-source this file and pass Get-MsvcPolicyCMakeArgs to CMake configure.
    The flag values live in msvc-release-policy.json, which build_ffmpeg_msvc.py
    reads as well, so every external build shares one policy.

    ISA and hardening flags are passed through the configuration-independent
    CMAKE_<LANG>_FLAGS and CMAKE_<TYPE>_LINKER_FLAGS variables on purpose:
    several patched upstream projects (Imath, OpenEXR, libavif, jxrlib,
    libdeflate, OpenJPH) overwrite CMAKE_<LANG>_FLAGS_RELEASE, which silently
    dropped /guard:cf and /Qspectre when the policy was passed there. Module
    linker flags are included because the Qt image plugins are MODULE targets.

    /MD stays in the Release baseline: projects whose cmake_minimum_required
    predates CMP0091 do not get a CRT selection from CMAKE_MSVC_RUNTIME_LIBRARY.

    The instruction set is a parameter (-Isa AVX2|SSE2). Each ISA has its own
    /arch flag and its own folder suffix (see the "isa" map in the policy
    file), so AVX2 and SSE2 dependencies never share an install or build
    folder. Use Get-IsaFolderName for every ISA-specific folder name; the
    suffixes match QIMGV_DEPS_SUFFIX in cmake/QimgvMsvcPolicy.cmake.
#>

$MSVC_POLICY_FILE = Join-Path $PSScriptRoot "msvc-release-policy.json"

# The ISA whose binaries the shared Qt SDK and the plain release/ folder hold.
# Scripts only deploy into the Qt SDK for this ISA.
$MSVC_DEFAULT_ISA = "AVX2"

# CMake's own MSVC defaults, which are replaced when CMAKE_<LANG>_FLAGS and
# CMAKE_<TYPE>_LINKER_FLAGS are given on the command line.
$MSVC_CMAKE_DEFAULT_C_FLAGS   = "/DWIN32 /D_WINDOWS"
$MSVC_CMAKE_DEFAULT_CXX_FLAGS = "/DWIN32 /D_WINDOWS /EHsc"
$MSVC_CMAKE_DEFAULT_LINKER_FLAGS = "/machine:x64"

function Get-MsvcPolicy {
    if (-not (Test-Path $MSVC_POLICY_FILE)) {
        throw "MSVC policy file not found: $MSVC_POLICY_FILE"
    }
    $policy = Get-Content -Path $MSVC_POLICY_FILE -Raw | ConvertFrom-Json
    foreach ($key in @("release_baseline", "compile", "link")) {
        if (-not $policy.$key) {
            throw "MSVC policy file $MSVC_POLICY_FILE is missing '$key'"
        }
    }
    if (-not $policy.isa) {
        throw "MSVC policy file $MSVC_POLICY_FILE is missing 'isa'"
    }
    return $policy
}

function Get-MsvcIsaPolicy {
    param([Parameter(Mandatory)] [string] $Isa)

    $isaPolicy = (Get-MsvcPolicy).isa.$Isa
    if (-not $isaPolicy) {
        throw "MSVC policy file $MSVC_POLICY_FILE has no 'isa' entry for '$Isa'"
    }
    if (-not $isaPolicy.arch) {
        throw "MSVC policy file $MSVC_POLICY_FILE is missing 'isa.$Isa.arch'"
    }
    # An empty suffix is valid (the default ISA keeps the plain folder names).
    if ($null -eq $isaPolicy.deps_suffix) {
        throw "MSVC policy file $MSVC_POLICY_FILE is missing 'isa.$Isa.deps_suffix'"
    }
    return $isaPolicy
}

# ISA flag followed by the hardening flags, in the order the AVX2 build used
# before the ISA became a parameter.
function Get-MsvcPolicyCompileFlags {
    param([Parameter(Mandatory)] [string] $Isa)

    return "$((Get-MsvcIsaPolicy $Isa).arch) $((Get-MsvcPolicy).compile)"
}

# Folder name for an ISA-specific install, build or output folder:
# "install" -> "install" (AVX2) / "install-sse2" (SSE2).
function Get-IsaFolderName {
    param(
        [Parameter(Mandatory)] [string] $BaseName,
        [Parameter(Mandatory)] [string] $Isa
    )

    return "$BaseName$((Get-MsvcIsaPolicy $Isa).deps_suffix)"
}

function Get-MsvcPolicyCMakeArgs {
    param(
        [Parameter(Mandatory)] [string] $Isa,
        [switch] $Ipo
    )

    $policy = Get-MsvcPolicy
    $compileFlags = Get-MsvcPolicyCompileFlags $Isa
    $linkerFlags = "$MSVC_CMAKE_DEFAULT_LINKER_FLAGS $($policy.link)"

    $cmakeArgs = @(
        "-DCMAKE_C_FLAGS=$MSVC_CMAKE_DEFAULT_C_FLAGS $compileFlags",
        "-DCMAKE_CXX_FLAGS=$MSVC_CMAKE_DEFAULT_CXX_FLAGS $compileFlags",
        "-DCMAKE_C_FLAGS_RELEASE=$($policy.release_baseline)",
        "-DCMAKE_CXX_FLAGS_RELEASE=$($policy.release_baseline)",
        "-DCMAKE_EXE_LINKER_FLAGS=$linkerFlags",
        "-DCMAKE_SHARED_LINKER_FLAGS=$linkerFlags",
        "-DCMAKE_MODULE_LINKER_FLAGS=$linkerFlags",
        "-DCMAKE_STATIC_LINKER_FLAGS_RELEASE="
    )
    if ($Ipo) {
        $cmakeArgs += "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=ON"
    }
    return $cmakeArgs
}
