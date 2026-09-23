# Building qimgv-plus from Source

This guide walks you through building qimgv-plus and all its dependencies on Windows.

## Prerequisites

| Tool | Version | Download | Notes |
|------|---------|----------|-------|
| **Visual Studio** | 2022 (17.x+) | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) | "Desktop development with C++" workload |
| **CMake** | 3.25+ | Included with VS or [cmake.org](https://cmake.org/download/) | |
| **Git** | Latest | [git-scm.com](https://git-scm.com/) | |
| **Qt SDK** | 6.11+ | [qt.io](https://www.qt.io/download-qt-installer) | Components: Core, Widgets, Network, Svg, SvgWidgets, PrintSupport, OpenGLWidgets, Sql, Pdf, LinguistTools |
| **Vulkan SDK** | Latest | [vulkan.lunarg.com](https://vulkan.lunarg.com/sdk/home) | Required for upscayl-ncnn (AI upscaling) |
| **MSYS2** | Latest | [msys2.org](https://www.msys2.org/) | Only needed for building FFmpeg (provides bash/make) |
| **Python 3** | 3.10+ | [python.org](https://www.python.org/) | Only needed for building FFmpeg |

### Minimum Hardware

- **CPU**: x86-64 with AVX2 support
- **RAM**: 16 GB
- **GPU VRAM**: 8 GB
- **Storage**: SSD
- **OS**: Windows 10 64-bit or newer

---

## Quick Start (Automated)

```powershell
# 1. Clone the main repository
git clone https://github.com/hadoooooouken/qimgv-plus.git
cd qimgv-plus

# 2. Run the dependency setup script (clones all deps + applies patches)
.\build_scripts\setup-deps.ps1

# 3. Manually download exiv2 and NASM (see "Manual Downloads" below)

# 4. Build format libraries (from a VS Developer Command Prompt)
cd build_scripts
.\build_qtiff_jpeg.ps1          # zlib-ng -> libjpeg-turbo -> libtiff -> qtiff.dll
.\build_qpng_spng.ps1           # libspng
.\build_qjpeg_jpeg.ps1          # qjpeg.dll
.\rebuild-all.ps1               # Imath, OpenEXR, libavif, libjxl, jxrlib, libdeflate, zstd, LibRaw
python build_ffmpeg_msvc.py     # FFmpeg HEVC decoder (optional, requires MSYS2)
cd ..

# 5. Configure and build
cmake --preset qimgv-x64-release
cmake --build out/build/qimgv-x64-release --config Release
```

> [!NOTE]
> Steps 4-5 require a **Visual Studio Developer Command Prompt** (or running `vcvarsall.bat x64` first) so that `cl.exe`, `link.exe`, and related tools are on PATH.

---

## Manual Downloads

These components must be downloaded manually and placed into `formats/`:

### exiv2 (prebuilt binary)

1. Download the official Visual Studio 2022 bundle from the [Exiv2 releases page](https://github.com/Exiv2/exiv2/releases/tag/v0.28.9)
   - File: `exiv2-0.28.9-2019msvc64.zip`
2. Extract to `formats/exiv2/` so that `formats/exiv2/bin/exiv2.dll` exists

### NASM (assembler)

Required by libjpeg-turbo for SIMD optimizations.

1. Download from [nasm.us](https://www.nasm.us/pub/nasm/releasebuilds/) (Windows 64-bit ZIP)
2. Place `nasm.exe` into `formats/nasm/`

### Ninja (build tool)

Used by CMake presets for the main build.

1. Download from [Ninja releases](https://github.com/ninja-build/ninja/releases)
2. Place `ninja.exe` into `formats/ninja/`

---

## Repository Layout

```
qimgv-plus/
├── qimgv/                     # Application source code
├── shellex/                   # Windows shell extension
├── upscayl-ncnn/              # AI upscaling engine (separate repo)
├── formats/                   # Third-party dependency sources
│   ├── Imath/                 # OpenEXR math library
│   ├── openexr/               # OpenEXR image format
│   ├── libavif/               # AVIF codec (with aom + dav1d)
│   ├── libjxl/                # JPEG XL codec
│   ├── jxrlib/                # JPEG XR codec
│   ├── libdeflate/            # Fast DEFLATE compression
│   ├── zstd/                  # Zstandard compression
│   ├── LibRaw/                # RAW image decoder
│   ├── zlib-ng/               # zlib replacement (shared DLL)
│   ├── libjpeg-turbo/         # JPEG codec with SIMD (shared DLL)
│   ├── libtiff/               # TIFF codec (static, links libjpeg-turbo)
│   ├── libspng/               # PNG codec (static, links zlib-ng)
│   ├── ffmpeg/                # FFmpeg (HEVC decoder only)
│   ├── kimageformats/         # KDE image format plugins
│   ├── openjpeg/              # JPEG 2000 codec
│   ├── OpenJPH/               # HTJ2K codec
│   ├── exiv2/                 # EXIF metadata (prebuilt binary)
│   ├── nasm/                  # NASM assembler (tool binary)
│   ├── ninja/                 # Ninja build tool (tool binary)
│   └── magic-kernel-sharp/    # Scaling kernel data
├── build_scripts/             # Dependency build automation
│   ├── rebuild-all.ps1
│   ├── build_qtiff_jpeg.ps1
│   ├── build_qpng_spng.ps1
│   ├── build_qjpeg_jpeg.ps1
│   ├── build_ffmpeg_msvc.py
│   ├── check_deps.py
│   ├── deploy-plugins.ps1
│   └── setup-deps.ps1
├── patches/                   # Local patch files for dependencies
│   ├── *.patch                # Single-file patches
│   ├── kimageformats/         # Patch series (10 commits past v6.26.0)
│   └── openjpeg/              # Patch series (15 commits past v2.5.4)
```

---

## Dependency Table

### Built by `rebuild-all.ps1`

| Library | Version | Upstream | Patched | Output |
|---------|---------|----------|:---:|--------|
| Imath | v3.2.3 | [AcademySoftwareFoundation/Imath](https://github.com/AcademySoftwareFoundation/Imath) | Yes | Static lib |
| OpenEXR | v3.4.15 | [AcademySoftwareFoundation/openexr](https://github.com/AcademySoftwareFoundation/openexr) | Yes | Static lib |
| libavif | v1.4.2 | [AOMediaCodec/libavif](https://github.com/AOMediaCodec/libavif) | Yes | Static lib |
| libjxl | v0.12.0 | [libjxl/libjxl](https://github.com/libjxl/libjxl) | No | Static lib |
| jxrlib | v2019.10.9 | [4creators/jxrlib](https://github.com/4creators/jxrlib) | Yes | Static lib |
| libdeflate | 1.26 | [ebiggers/libdeflate](https://github.com/ebiggers/libdeflate) | Yes | Shared DLL (`deflate.dll`) |
| zstd | 1.5.7 | [facebook/zstd](https://github.com/facebook/zstd) | No | Shared DLL (`zstd.dll`) |
| LibRaw | 0.22.2 | [LibRaw/LibRaw](https://github.com/LibRaw/LibRaw) | Yes | Shared DLL (`libraw.dll`) |

### Built by `build_qtiff_jpeg.ps1` / `build_qpng_spng.ps1` / `build_qjpeg_jpeg.ps1`

| Library | Version | Upstream | Patched | Output |
|---------|---------|----------|:---:|--------|
| zlib-ng | 2.3.3 | [zlib-ng/zlib-ng](https://github.com/zlib-ng/zlib-ng) | No | Shared DLL (`zlib1.dll`) |
| libjpeg-turbo | 3.2.0 | [libjpeg-turbo/libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo) | No | Shared DLL (`jpeg62.dll`) |
| libtiff | 4.7.2 | [libtiff/libtiff](https://gitlab.com/libtiff/libtiff) | No | Static lib |
| libspng | 0.7.4 | [randy408/libspng](https://github.com/randy408/libspng) | No | Static lib |
| qtiff.dll | — | Qt plugin | Custom build | Qt image format plugin |
| qjpeg.dll | — | Qt plugin | Custom build | Qt image format plugin |

### Built as part of main CMake tree

| Library | Version | Upstream | Patched | Notes |
|---------|---------|----------|:---:|-------|
| kimageformats | v6.26.0 + 10 commits | [KDE/kimageformats](https://invent.kde.org/frameworks/kimageformats) | Yes | Qt image format plugins (kimg_*.dll) |
| OpenJPEG | v2.5.4 + 15 commits | [uclouvain/openjpeg](https://github.com/uclouvain/openjpeg) | Yes | JPEG 2000 codec |
| OpenJPH | 0.31.0 | [aous72/OpenJPH](https://github.com/aous72/OpenJPH) | Yes | HTJ2K codec |
| upscayl-ncnn | (fork) | [hadoooooouken/upscayl-ncnn-qimgv-plus](https://github.com/hadoooooouken/upscayl-ncnn-qimgv-plus) | Fork | AI upscaling engine |

### Built separately

| Library | Version | Upstream | Notes |
|---------|---------|----------|-------|
| FFmpeg | n9.0.1 | [FFmpeg](https://git.ffmpeg.org/ffmpeg.git) | HEVC decoder only; requires MSYS2 + Python |

### Prebuilt / Tools (no compilation needed)

| Component | Version | Source |
|-----------|---------|--------|
| exiv2 | 0.28.9 | [Exiv2 releases](https://github.com/Exiv2/exiv2/releases/tag/v0.28.9) (VS2022 bundle) |
| NASM | Latest | [nasm.us](https://www.nasm.us/) |
| Ninja | Latest | [ninja-build/ninja](https://github.com/ninja-build/ninja/releases) |

---

## Patch Descriptions

All patches are in the `patches/` directory. They are applied automatically by `build_scripts/setup-deps.ps1`.

| Patch File | Library | Description |
|------------|---------|-------------|
| `Imath-v3.2.3-avx2-flags.patch` | Imath | Adds MSVC AVX2, `/O2` optimization, and LTCG flags |
| `libavif-v1.4.2-avx2-flags.patch` | libavif | Adds MSVC AVX2, `/O2` optimization, and LTCG flags |
| `openexr-v3.4.15-avx2-flags.patch` | OpenEXR | Adds MSVC AVX2, `/O2` optimization, and LTCG flags |
| `OpenJPH-0.31.0-avx2-flags.patch` | OpenJPH | Adds MSVC AVX2, `/O2` optimization, and LTCG flags |
| `libdeflate-1.26-avx2-flags.patch` | libdeflate | Adds MSVC AVX2/LTCG block for standalone builds |
| `jxrlib-v2019.10.9-uintptr-fix.patch` | jxrlib | Fixes encoding issues and uses portable `uintptr_t` |
| `LibRaw-0.22.2-vcxproj-toolset.patch` | LibRaw | Updates VS project files to toolset v145 and SDK 10.0 |
| `kimageformats/0001-*.patch` ... | kimageformats | 10 upstream commits cherry-picked past v6.26.0 (HEIF transforms, IFF DEEP, Farbfeld support, bug fixes) |
| `openjpeg/0001-*.patch` ... | openjpeg | 15 upstream commits past v2.5.4 (bug fixes, NEON optimizations, ARM64 support) |

---

## Build Order (Detailed)

### Phase 1: Format Libraries

All scripts should be run from a **VS Developer Command Prompt** (x64).

#### Step 1: `build_qtiff_jpeg.ps1`

Builds the image I/O foundation used by the entire application:

```powershell
cd build_scripts
.\build_qtiff_jpeg.ps1
```

**Chain:** zlib-ng (shared) → libjpeg-turbo (shared, NASM SIMD) → libtiff (static) → qtiff.dll (Qt plugin)

**Outputs:**
- `formats/zlib-ng/install/` → `zlib1.dll`
- `formats/libjpeg-turbo/install/` → `jpeg62.dll`
- `formats/libtiff/install/` → static lib
- `build_scripts/qtiff_jpeg/install/` → `qtiff.dll`

#### Step 2: `build_qpng_spng.ps1`

```powershell
.\build_qpng_spng.ps1
```

Builds libspng (static). Requires zlib-ng from Step 1.

**Output:** `formats/libspng/install/` → static lib

#### Step 3: `build_qjpeg_jpeg.ps1`

```powershell
.\build_qjpeg_jpeg.ps1
```

Builds a custom qjpeg.dll backed by the shared libjpeg-turbo from Step 1.

**Output:** `build_scripts/qjpeg_jpeg/install/` → `qjpeg.dll`

#### Step 4: `rebuild-all.ps1`

```powershell
.\rebuild-all.ps1
```

Builds all remaining format libraries in dependency order: Imath → OpenEXR → libavif → libjxl → jxrlib → libdeflate → zstd → LibRaw.

#### Step 5 (Optional): `build_ffmpeg_msvc.py`

```powershell
python build_ffmpeg_msvc.py
```

Builds a minimal FFmpeg with only the HEVC decoder. Required for HEIF/HEIC support via the `kimg_heif` plugin.

**Requires:** MSYS2, Python 3, NASM

> [!NOTE]
> If you skip this step, the application will still build but HEIF/HEIC format support may be limited.

### Phase 2: Main Application

```powershell
cd ..  # back to project root

# Configure (using presets)
cmake --preset qimgv-x64-release

# Build
cmake --build out/build/qimgv-x64-release --config Release
```

The built application and all plugins/DLLs are placed in the `release/` directory.

### Phase 3: Post-Build (Optional)

```powershell
cd build_scripts
.\deploy-plugins.ps1    # Copy kimg_*.dll plugins to release/imageformats/
```

---

## Path Configuration

The build scripts and CMake presets reference several SDK installation paths. By default they use the paths below. If your SDKs are installed elsewhere, set the corresponding **environment variables** before running any build step.

| Purpose | Environment Variable | Default Value |
|---------|---------------------|---------------|
| Qt SDK | `QT_DIR` | `E:\Qt\6.11.2\msvc2022_64` |
| Vulkan SDK | `VULKAN_SDK` | `E:\VULKAN` |
| Visual Studio | `VSINSTALLDIR` | Auto-detected via `vswhere.exe` |
| MSYS2 (FFmpeg only) | `MSYS2_ROOT` | `E:\MSYS2` |
| Python (FFmpeg only) | — | System PATH |

### CMakePresets.json

Edit [`CMakePresets.json`](CMakePresets.json) to update these values for your system:

```json
{
  "cacheVariables": {
    "CMAKE_PREFIX_PATH": "E:/Qt/6.11.2/msvc2022_64;E:/Vulkan"
  },
  "environment": {
    "VULKAN_SDK": "E:/Vulkan"
  }
}
```

### Build Scripts

The following scripts have Qt SDK paths that may need updating:

| Script | Variable | Line |
|--------|----------|------|
| `build_qtiff_jpeg.ps1` | `$QT_DIR` | Line 54 |
| `build_qjpeg_jpeg.ps1` | `$QT_DIR` | Line 53 |
| `build_ffmpeg_msvc.py` | `MSVC_BASE`, `MSYS2_BIN` | Lines 10-11 |

---

## Troubleshooting

### "cl.exe not found" errors

Make sure you're running from a **VS Developer Command Prompt**, or run:
```cmd
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
```

### "NASM not found" during libjpeg-turbo build

Ensure `nasm.exe` is in `formats/nasm/`. The build scripts reference this path directly.

### Vulkan SDK not found

Set the `VULKAN_SDK` environment variable:
```powershell
$env:VULKAN_SDK = "C:\VulkanSDK\1.3.xxx.x"
```

### Qt not found during CMake configure

Update `CMAKE_PREFIX_PATH` in `CMakePresets.json` to point to your Qt installation:
```
E:/Qt/6.x.x/msvc2022_64
```

### zlib-ng import library not found

Run `build_scripts\build_qtiff_jpeg.ps1` first — it builds zlib-ng, which is required by both the main application and libspng.
