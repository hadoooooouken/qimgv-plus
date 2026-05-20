import os
import subprocess
import sys
from pathlib import Path

# ==================== PATH SETTINGS ====================
MSVC_VERSION = "14.51.36231"
WINDOWS_SDK_VERSION = "10.0.26100.0"

MSVC_BASE = Path("E:/MVSC")
MSYS2_BIN = Path("E:/MSYS2/usr/bin/bash.exe")
FFMPEG_SRC = Path("E:/qimgv/formats/ffmpeg")
PREFIX = FFMPEG_SRC / "ffmpeg-build-msvc"
# =========================================================

def run_build():
    print(f"[-] Script location: {__file__}")
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
    
    env["PATH"] = f"{msvc_bin};{msvc_ide};{env['PATH']}"
    env["INCLUDE"] = ";".join(str(p) for p in [MSVC_BASE / f"VC/Tools/MSVC/{MSVC_VERSION}/include", sdk_include / "ucrt", sdk_include / "shared", sdk_include / "um"])
    env["LIB"] = ";".join(str(p) for p in [MSVC_BASE / f"VC/Tools/MSVC/{MSVC_VERSION}/lib/x64", sdk_lib / "ucrt" / "x64", sdk_lib / "um" / "x64"])
    
    env["CC"] = "cl"
    env["CXX"] = "cl"
    env["AR"] = "lib"

    print("[+] Paths translated to native Unix format.")
    print("[-] Launching clean build engine...")

    # 4. Pure execution instructions inside Bash
    bash_script = f"""
    set -euo pipefail
    
    export PATH="{msvc_bin_msys}:{msvc_ide_msys}:/usr/local/bin:/usr/bin:/bin"
    export INCLUDE="{':'.join(inc_list)}"
    export LIB="{':'.join(lib_list)}"
    
    cd "{FFMPEG_SRC.as_posix()}"
    mkdir -p "{PREFIX.as_posix()}"
    
    echo "[-] Running FFmpeg ./configure..."
    ./configure \\
      --prefix="{PREFIX.as_posix()}" \\
      --toolchain=msvc \\
      --arch=x86_64 \\
      --target-os=win32 \\
      --enable-static \\
      --disable-shared \\
      --disable-everything \\
      --disable-network \\
      --disable-autodetect \\
      --disable-hwaccels \\
      --disable-x86asm \\
      --enable-decoder=hevc \\
      --enable-parser=hevc \\
      --enable-demuxer=mov,mp4 \\
      --enable-protocol=file
      
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
        print(f"\n[+] SUCCESS! Build finished.\n    Target directory: {PREFIX}")
    else:
        print(f"\n[!] Build execution failed. Exit code: {process.returncode}")

if __name__ == "__main__":
    run_build()