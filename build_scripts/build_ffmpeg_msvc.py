import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

# ==================== PATH SETTINGS ====================
MSVC_VERSION = "14.51.36231"
WINDOWS_SDK_VERSION = "10.0.26100.0"

MSVC_BASE = Path(os.environ.get("VSINSTALLDIR", "E:/MVSC"))
MSYS2_BIN = Path(os.environ.get("MSYS2_ROOT", "E:/MSYS2")) / "usr/bin/bash.exe"
FFMPEG_SRC = Path(os.environ.get("FFMPEG_SRC", "E:/qimgv/formats/ffmpeg"))
# The install prefix gets the ISA folder suffix from the policy file
# (ffmpeg-build-msvc for AVX2, ffmpeg-build-msvc-sse2 for SSE2).
PREFIX_BASE_NAME = "ffmpeg-build-msvc"
NASM_DIR = Path(os.environ.get("NASM_DIR", "E:/qimgv/formats/nasm"))

# ISA / hardening flags come from the policy file shared with the PowerShell
# build scripts. FFmpeg's configure adds -O2 itself, so only the CRT selection
# is taken from the Release baseline: without -MD the static libraries default
# to the static CRT (/DEFAULTLIB:LIBCMT) while kimg_heif links the DLL CRT.
MSVC_POLICY_FILE = Path(__file__).resolve().parent / "msvc-release-policy.json"
MSVC_DLL_CRT_FLAG = "-MD"
MSVC_DEFAULT_ISA = "AVX2"


def to_dash_options(flags):
    # MSYS2 rewrites arguments that start with "/" into Windows paths, so MSVC
    # options are passed in their equivalent "-" form.
    return " ".join("-" + flag[1:] if flag.startswith("/") else flag for flag in flags.split())


def load_msvc_policy():
    with open(MSVC_POLICY_FILE, encoding="utf-8") as policy_file:
        policy = json.load(policy_file)
    for key in ("compile", "link", "isa"):
        if not policy.get(key):
            raise KeyError(f"MSVC policy file {MSVC_POLICY_FILE} is missing '{key}'")
    return policy


def get_isa_policy(policy, isa):
    isa_policy = policy["isa"].get(isa)
    if not isa_policy:
        raise KeyError(f"MSVC policy file {MSVC_POLICY_FILE} has no 'isa' entry for '{isa}'")
    if not isa_policy.get("arch"):
        raise KeyError(f"MSVC policy file {MSVC_POLICY_FILE} is missing 'isa.{isa}.arch'")
    # An empty suffix is valid (the default ISA keeps the plain folder names).
    if isa_policy.get("deps_suffix") is None:
        raise KeyError(f"MSVC policy file {MSVC_POLICY_FILE} is missing 'isa.{isa}.deps_suffix'")
    return isa_policy


MSVC_POLICY = load_msvc_policy()
LINK_HARDENING_FLAGS = to_dash_options(MSVC_POLICY["link"])
# =========================================================

def run_build(isa):
    isa_policy = get_isa_policy(MSVC_POLICY, isa)
    prefix = FFMPEG_SRC / f"{PREFIX_BASE_NAME}{isa_policy['deps_suffix']}"
    # ISA flag first, then the hardening flags (same order as MsvcPolicy.ps1).
    cl_policy_flags = (f"{MSVC_DLL_CRT_FLAG} {to_dash_options(isa_policy['arch'])} "
                       f"{to_dash_options(MSVC_POLICY['compile'])}")

    print(f"[-] Script location: {__file__}")
    print(f"[-] ISA: {isa} (prefix: {prefix})")
    print("[-] Preparing paths and environment...")
    
    # 1. Verify critical components
    msvc_bin = MSVC_BASE / f"VC/Tools/MSVC/{MSVC_VERSION}/bin/Hostx64/x64"
    msvc_ide = MSVC_BASE / "Common7/IDE"
    
    sdk_base = Path("E:/Windows Kits/10")
    if not sdk_base.exists():
        sdk_base = Path("C:/Program Files (x86)/Windows Kits/10")
        
    sdk_include = sdk_base / "Include" / WINDOWS_SDK_VERSION
    sdk_lib = sdk_base / "Lib" / WINDOWS_SDK_VERSION

    if not msvc_bin.exists() or not MSYS2_BIN.exists():
        print("[!] Error: Check MSVC or MSYS2 paths installation!")
        sys.exit(1)

    # 2. Convert Windows paths to MSYS2-compatible format directly in Python
    def to_msys_path(path_obj):
        p = path_obj.as_posix()
        if p and p[1] == ':':
            return f"/{p[0].lower()}{p[2:]}"
        return p

    msvc_bin_msys = to_msys_path(msvc_bin)
    msvc_ide_msys = to_msys_path(msvc_ide)
    nasm_dir_msys = to_msys_path(NASM_DIR)

    if not NASM_DIR.exists():
        print(f"[!] Error: NASM not found at {NASM_DIR} -- required for x86 SIMD asm")
        sys.exit(1)

    # Construct clean, native MSYS2 INCLUDE and LIB representations
    inc_list = [
        to_msys_path(MSVC_BASE / f"VC/Tools/MSVC/{MSVC_VERSION}/include"),
        to_msys_path(sdk_include / "ucrt"),
        to_msys_path(sdk_include / "shared"),
        to_msys_path(sdk_include / "um")
    ]
    
    lib_list = [
        to_msys_path(MSVC_BASE / f"VC/Tools/MSVC/{MSVC_VERSION}/lib/x64"),
        to_msys_path(sdk_lib / "ucrt" / "x64"),
        to_msys_path(sdk_lib / "um" / "x64")
    ]

    # 3. Setup environment variables for the process execution
    env = os.environ.copy()
    
    env["PATH"] = f"{msvc_bin};{msvc_ide};{NASM_DIR};{env['PATH']}"
    env["INCLUDE"] = ";".join(str(p) for p in [MSVC_BASE / f"VC/Tools/MSVC/{MSVC_VERSION}/include", sdk_include / "ucrt", sdk_include / "shared", sdk_include / "um"])
    env["LIB"] = ";".join(str(p) for p in [MSVC_BASE / f"VC/Tools/MSVC/{MSVC_VERSION}/lib/x64", sdk_lib / "ucrt" / "x64", sdk_lib / "um" / "x64"])
    
    env["CC"] = "cl"
    env["CXX"] = "cl"
    env["AR"] = "lib"

    # Force MSYS2 to keep our Windows paths
    env["MSYS2_PATH_TYPE"] = "inherit"

    print("[+] Paths translated to native Unix format.")
    print("[-] Launching clean build engine...")

    # 4. Pure execution instructions inside Bash
    bash_script = f"""
    set -euo pipefail
    
    # Restore the essential MSYS2 tool paths, while keeping the inherited MSVC paths!
    export PATH="/usr/local/bin:/usr/bin:/bin:$PATH"
    
    cd "{FFMPEG_SRC.as_posix()}"
    mkdir -p "{prefix.as_posix()}"

    echo "[-] Running FFmpeg ./configure..."
    ./configure \\
      --prefix="{prefix.as_posix()}" \\
      --toolchain=msvc \\
      --arch=x86_64 \\
      --target-os=win32 \\
      --enable-static \\
      --disable-shared \\
      --disable-everything \\
      --disable-network \\
      --disable-autodetect \\
      --disable-hwaccels \\
      --enable-decoder=hevc \\
      --enable-parser=hevc \\
      --enable-demuxer=mov,mp4 \\
      --enable-protocol=file \\
      --extra-cflags="{cl_policy_flags}" \\
      --extra-cxxflags="{cl_policy_flags} /EHsc" \\
      --extra-ldflags="{LINK_HARDENING_FLAGS}"
      
    echo "[-] Cleaning workspace..."
    make clean || true
    
    echo "[-] Building static components..."
    make -j$(nproc)
    make install
    """

    # 5. Execute via clean shell fork
    process = subprocess.Popen(
        [str(MSYS2_BIN), "--noprofile", "--norc", "-c", bash_script],
        env=env,
        stdout=sys.stdout,
        stderr=sys.stderr,
        text=True
    )
    process.wait()

    if process.returncode == 0:
        print(f"\n[+] SUCCESS! Build finished.\n    Target directory: {prefix}")
    else:
        print(f"\n[!] Build execution failed. Exit code: {process.returncode}")
        sys.exit(process.returncode)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build static FFmpeg (HEVC decoder) with MSVC for kimg_heif / kimg_ff.")
    parser.add_argument("--isa", choices=sorted(MSVC_POLICY["isa"]), default=MSVC_DEFAULT_ISA,
                        help="Instruction set baseline; selects the /arch flag and the install prefix suffix.")
    run_build(parser.parse_args().isa)
