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
#>

$MSVC_POLICY_FILE = Join-Path $PSScriptRoot "msvc-release-policy.json"

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
    return $policy
}

function Get-MsvcPolicyCMakeArgs {
    param([switch] $Ipo)

    $policy = Get-MsvcPolicy
    $linkerFlags = "$MSVC_CMAKE_DEFAULT_LINKER_FLAGS $($policy.link)"

    $cmakeArgs = @(
        "-DCMAKE_C_FLAGS=$MSVC_CMAKE_DEFAULT_C_FLAGS $($policy.compile)",
        "-DCMAKE_CXX_FLAGS=$MSVC_CMAKE_DEFAULT_CXX_FLAGS $($policy.compile)",
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
