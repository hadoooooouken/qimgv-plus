# SSE2 Release Build Plan

Goal: ship two installers from one source tree:

- `qimgv-plus-setup.exe`: the current AVX2 build, unchanged in behavior and performance.
- `qimgv-plus-setup-sse2.exe`: a build that runs on x64 CPUs without AVX/AVX2.

There is no separate long-lived branch. The ISA is a build parameter. Each variant has its own
CMake build directory (its own `.sln`), its own dependency install tree and its own output folder.

Work through the stages in order. Each stage is one atomic commit (or a few) and leaves the AVX2
build working. Tick the status box when a stage is merged.

## Key Facts

- The instruction set in a binary is decided by compiler flags, not by the build machine. An SSE2
  build compiled on an AVX2 machine runs on that machine too, because SSE2 is a subset of AVX2.
- On MSVC, removing `/arch:AVX2` is **not enough**. `_mm256_*` intrinsics still compile without
  the flag and crash with an illegal instruction on a CPU without AVX2. Every hand-written AVX2 path
  needs a compile-time alternative.
- MSVC predefines `__AVX2__` only under `/arch:AVX2`. That macro selects the code path.
  ISA-selection `#if` blocks are allowed. The AGENTS.md rule against conditional compilation covers
  OS/platform checks, not ISA checks.
- Every shipped DLL must be built for the same ISA. One AVX2-compiled dependency (libjpeg-turbo,
  LibRaw, ...) is enough to crash the SSE2 build on an old CPU.
- Windows 11 24H2 itself requires SSE4.2/POPCNT. The SSE2 build therefore matters mainly for
  Windows 10 machines.

## Current ISA Configuration

| Area | Location | Current value |
|---|---|---|
| First-party targets | `cmake/QimgvMsvcPolicy.cmake:25` (`QIMGV_MSVC_ISA_OPTIONS`) | `/arch:AVX2` |
| ncnn base TUs | `CMakeLists.txt:84` (application ISA placed before ncnn's own `/arch:SSE2`) | AVX2 macros, runtime dispatch |
| glslang | `CMakeLists.txt:86-90` via `qimgv_apply_msvc_profile` | `/arch:AVX2` |
| External dependencies | `build_scripts/msvc-release-policy.json` (`isa.<ISA>.arch`, Stage 2) | `/arch:AVX2` (default `-Isa`) |
| Dependency install trees | `formats/<lib>/install` (hardcoded in CMake files, see Stage 1) | single tree |
| Output folder | `qimgv/CMakeLists.txt:135`, `shellex/CMakeLists.txt:39` | `../release` |
| Installer CPU check | `qimgv/qimgv-plus.iss`, `InitializeSetup` | aborts without AVX2 |

Hand-written AVX2 code:

| File | AVX2 code | Existing scalar code |
|---|---|---|
| `qimgv/utils/hdrtonemapper.cpp` | primitives at lines 222-525, worker at 640+ | `*Scalar` helpers (lines 528-640) for row tails |
| `qimgv/utils/imagelib.cpp` | MKS fixed/general passes (130-390), `scaled_Smart`, `applyColorAdjustments` | scalar tails and the "Scalar fallback" loops |
| `qimgv/3rdparty/kimageformats/hdr.cpp` | `storeRgbePairAvx2` and the RGBE block loop (346-388) | `convertRgbeScalar` (330-344), full per-pixel path |
| `qimgv/3rdparty/djvudec/djvu.c` | already guarded by `#if defined(__AVX2__)` | already present |

### Stage 0 Findings

Audited 2026-10-11 against the `release/` folder from the 2026-10-10 build. Runtime-dispatched
means that the code reaches AVX/AVX2 only after a CPUID check.

Shipped binaries and their producers:

| Binary | Producer | ISA source |
|---|---|---|
| `qimgv-plus.exe` | target `qimgv-plus` (`qimgv/CMakeLists.txt`) | `QIMGV_MSVC_ISA_OPTIONS`; statically links exiv2 (`build_exiv2.ps1`, policy JSON) |
| `qimgvshellex.dll` | target `qimgvshellex` (`shellex/CMakeLists.txt`) | `QIMGV_MSVC_ISA_OPTIONS`; statically links `libraw_static.lib` (`rebuild-all.ps1` LibRaw) |
| `upscayl-ncnn.dll` | target `upscayl-ncnn` (root `CMakeLists.txt`) | `QIMGV_MSVC_ISA_OPTIONS` + ncnn runtime dispatch + glslang |
| `imageformats/kimg_*.dll` | `kimageformats_add_plugin` targets, copied by `qimgv-plus` POST_BUILD (also `deploy-plugins.ps1`) | `QIMGV_MSVC_ISA_OPTIONS` + statically linked codecs, see below |
| `imageformats/qjpeg.dll` | `build_qjpeg_jpeg.ps1` -> `build_scripts/qjpeg_jpeg/install`, copied by POST_BUILD over the stock plugin | policy JSON |
| `imageformats/qtiff.dll` | `build_qtiff_jpeg.ps1` -> `build_scripts/qtiff_jpeg/install`, copied by POST_BUILD over the stock plugin | policy JSON (libtiff statically linked) |
| `jpeg62.dll` | `build_qtiff_jpeg.ps1` step `libjpeg-turbo` | policy JSON + NASM SIMD, runtime-dispatched |
| `zlib1.dll` | `build_qtiff_jpeg.ps1` step `zlib` (zlib-ng, `ZLIB_COMPAT=ON`) | policy JSON, runtime-dispatched |
| `deflate.dll` | `rebuild-all.ps1` libdeflate | policy JSON, runtime-dispatched |
| `zstd.dll` | `rebuild-all.ps1` zstd | policy JSON (zstd's x64 asm/BMI2 paths are runtime-dispatched) |
| `vcomp140.dll` | POST_BUILD copy from the MSVC redist | Microsoft, SSE2 baseline |
| `Qt6*.dll`, `platforms/`, `styles/`, `iconengines/`, `generic/`, `networkinformation/`, `sqldrivers/`, stock `imageformats/q*.dll` | `windeployqt` POST_BUILD from `E:/Qt/6.12.0/msvc2022_64` | Qt SDK, see below |
| `d3dcompiler_47.dll`, `dxcompiler.dll`, `dxil.dll` | `windeployqt` | Microsoft redistributables, SSE2 baseline |

Statically linked codecs inside the plugins (all built with the policy JSON flags, so all need an
SSE2 rebuild):

| Plugin | Static dependency | Build script | Path used by CMake |
|---|---|---|---|
| `kimg_exr` | OpenEXR, Imath (+ `zstd.dll`, `deflate.dll`) | `rebuild-all.ps1` | `formats/{openexr,Imath,zstd}/install` |
| `kimg_raw` | LibRaw | `rebuild-all.ps1` (nmake) | `formats/LibRaw/buildfiles/release-x86_64` |
| `kimg_jxr` | jxrlib | `rebuild-all.ps1` | `formats/jxrlib/install` |
| `kimg_heif`, `kimg_ff` | FFmpeg (`--enable-static --disable-shared`, x86asm runtime-dispatched) | `build_ffmpeg_msvc.py` | absolute `E:/qimgv/formats/ffmpeg/ffmpeg-build-msvc` |
| `kimg_avif` | libavif + dav1d, aom (encoder), libyuv; all runtime-dispatched | `rebuild-all.ps1` | `formats/libavif/build/Release` (build tree, not install) |
| `kimg_jxl` | libjxl, brotli, Highway (runtime-dispatched) | `rebuild-all.ps1` | `formats/libjxl/build/...` (build tree, not install) |
| `kimg_jp2` | openjpeg | not in any script (setup-deps clones it; built by hand) | `formats/openjpeg/install` |
| `kimg_jph` | OpenJPH | `rebuild-all.ps1` | `formats/OpenJPH/install` |
| `kimg_png` | libspng (+ `zlib1.dll`) | `build_qpng_spng.ps1` | `formats/libspng/install` |
| `kimg_7z`, `kimg_rar` | LZMA SDK, UnRAR | compiled in-tree (`qimgv_7z_sdk`, `qimgv_unrar_sdk`) | `QIMGV_MSVC_ISA_OPTIONS` |

Qt SDK minimum ISA: **SSE2, no custom Qt build is needed.**

- `mkspecs/qmodule.pri` has an empty `QT_CPU_FEATURES.x86_64`. Qt was built without any `/arch`
  baseline beyond x86-64, so the startup "Incompatible processor" check requires only SSE2.
- `QT_COMPILER_SUPPORTS_SSE4_2/AVX2/AVX512*` in `qconfig.h` lists the runtime-dispatched variants
  Qt compiled, not a baseline.
- Disassembly cross-check of `Qt6Core.dll` (about 1.08M lines): 1 `popcnt`, 0 `crc32`,
  6 `pcmpestri`/`pcmpistri`, 10 `pshufb`, 220 `ymm` lines, 0 FMA. Instruction counts this low
  only occur in dispatched functions, so the baseline is SSE2.
- The SDE smoke test in Stage 7 is the final runtime confirmation.

ncnn: `NCNN_RUNTIME_CPU=ON` (option default in `upscayl-ncnn/src/ncnn/CMakeLists.txt:78`, and set
in `out/build/qimgv-x64-release/CMakeCache.txt`), with `NCNN_AVX`, `NCNN_FMA`, `NCNN_F16C`,
`NCNN_AVX2` and `NCNN_AVX512` on. Today's base TUs contain unguarded AVX2 code (the `__AVX2__`
macro is leaked by the leading `/arch:AVX2`, see `CMakeLists.txt:73-80`), so the current
`upscayl-ncnn.dll` would crash on a pre-AVX2 CPU even without the application code. With
`/arch:SSE2` the base TUs fall back to SSE2 and the dispatched TUs are unchanged.

`hdr.cpp`: a full scalar path exists. `convertRgbeScalar` already converts the row tail, and it is
the only path for the half-float output format (`HDR_HALF_QUALITY`). Stage 5 only needs to guard
`storeRgbePairAvx2` and the `if constexpr` block. `convertedPixelCount` then stays `0`, and the
scalar call covers the whole row.

`djvu.c`: confirmed safe. Under `__AVX2__` it enables `DJVU_IW44_AVX2` (including its SSSE3
`_mm_shuffle_epi8` code at 4481-4516). Otherwise `_M_X64` selects the SSE2-only path through
`<emmintrin.h>`.

Corrections to the later stages found during the audit:

1. **The `imagelib.cpp` "Scalar/SSE tails" are not SSE2.** The odd-pixel tails in all four MKS
   passes (lines 201-214, 250-261, 314-326, 368-378) and in the `scaled_Smart` Gaussian pass
   (1049-1113) use `_mm_cvtepu8_epi32` and `_mm_packus_epi32` (SSE4.1) and `_mm_fmadd_ps` and
   `_mm_fmsub_ps` (FMA3). MSVC compiles these without `/arch` and they fault on an SSE2 CPU.
   Stage 4 must replace them with true scalar helpers, used by the AVX2 tails and the `#else`
   path, instead of reusing them. The truly scalar tails are those of the `scaled_Smart` upscale
   passes (758+, 850+), its sharpen fallback (954-972) and the `applyColorAdjustments` fallback
   (1433-1450).
2. `scaled_Smart` has **four** AVX2 passes, and Stage 4 must guard all of them: upscale
   horizontal (713-757), upscale vertical (813-849), sharpen (909-951) and downscale Gaussian
   (1022-1094).
3. Stage 1 must also make these paths ISA-specific: `build_scripts/qtiff_jpeg/install` and
   `build_scripts/qjpeg_jpeg/install` (`qimgv/CMakeLists.txt:199, 212`), the libavif and libjxl
   **build** trees (`kimageformats/CMakeLists.txt:250-268`), the absolute FFmpeg prefix (line 234),
   LibRaw `buildfiles/release-x86_64` in `kimg_raw` (line 208) as well as in shellex, and the
   OpenMP DLL path. The OpenMP DLL is ISA-neutral and stays shared.
4. Stage 2: `build_qtiff_jpeg.ps1` and `build_qjpeg_jpeg.ps1` also **deploy into the shared Qt SDK**
   (`plugins/imageformats`, `bin/jpeg62.dll`). An SSE2 run must skip that deploy (use the existing
   `-SkipDeploy` switch, or force it under `-Isa SSE2`). Otherwise it overwrites the AVX2
   plugins that developers run against. openjpeg has no build script and needs one, or a
   documented manual `-Isa SSE2` procedure.
5. `windeployqt` also ships unused SQL drivers (`qsqlibase`, `qsqlmimer`, `qsqloci`, `qsqlpsql`,
   `qsqlodbc`). This does not affect the ISA work, but it is an easy size reduction for both
   installers.
6. The QML branch adds Qt Quick/QML DLLs and `qml/` through the same `windeployqt` call from the
   same SDK. The Qt conclusion above applies to them unchanged.

---

## Stage 0: Audit (no code changes)

Status: [x] (findings in "Stage 0 Findings" above)

1. List every binary that `release/` ships (`*.exe`, `*.dll`, `imageformats/*.dll`, other plugin
   folders) and map each one to the script or CMake target that produces it.
2. Confirm the minimum ISA of the Qt SDK in use (`E:/Qt/6.12.0/msvc2022_64`). QtCore aborts at
   startup with "Incompatible processor" when the CPU lacks features Qt was compiled for. If the
   official build requires more than SSE2, decide between a custom Qt build and raising the SSE
   target to SSE4.2 for the whole plan.
3. Confirm that ncnn is configured with `NCNN_RUNTIME_CPU=ON`, so the SSE2 build still uses AVX2
   kernels on modern CPUs.
4. For `hdr.cpp`, confirm whether a scalar path exists next to the AVX2 block loop.
5. Record the findings in this file (the "Current ISA Configuration" section).

Done when: every shipped binary has a known producer and ISA source, and the Qt question is
resolved.

## Stage 1: CMake ISA Option (first-party build)

Status: [ ]

Files: `cmake/QimgvMsvcPolicy.cmake`, `CMakeLists.txt`, `qimgv/CMakeLists.txt`,
`qimgv/utils/CMakeLists.txt`, `qimgv/3rdparty/kimageformats/CMakeLists.txt`,
`shellex/CMakeLists.txt`, `CMakePresets.json`.

1. Add a validated cache variable in `QimgvMsvcPolicy.cmake`:
   - `QIMGV_ISA` (`STRING`, default `AVX2`, `STRINGS` property `AVX2;SSE2`).
   - Fail with `FATAL_ERROR` on any other value.
   - Derive `QIMGV_MSVC_ISA_OPTIONS`: `/arch:AVX2` for AVX2, `/arch:SSE2` for SSE2.
2. Derive from `QIMGV_ISA`, in one place:
   - `QIMGV_DEPS_INSTALL_NAME`: `install` for AVX2, `install-sse2` for SSE2.
   - `QIMGV_RELEASE_DIR`: `${CMAKE_SOURCE_DIR}/release` for AVX2,
     `${CMAKE_SOURCE_DIR}/release-sse2` for SSE2.
3. Replace every hardcoded `formats/<lib>/install` path with
   `formats/<lib>/${QIMGV_DEPS_INSTALL_NAME}`. Known sites:
   - `qimgv/CMakeLists.txt`: 68, 83, 102, 290, 301, 319, 327
   - `qimgv/utils/CMakeLists.txt`: 3
   - `qimgv/3rdparty/kimageformats/CMakeLists.txt`: 185-187, 212, 286, 295, 307
   - `shellex/CMakeLists.txt:52`: LibRaw `buildfiles/release-x86_64`; needs an ISA-specific
     folder, defined together with Stage 2
   - `CMakePresets.json`: `exiv2_DIR`
4. Replace both `RUNTIME_OUTPUT_DIRECTORY_RELEASE` values with `QIMGV_RELEASE_DIR`.
5. Check `CMakeLists.txt:84`: with SSE2, `QIMGV_MSVC_ISA_OPTIONS` is `/arch:SSE2`, so ncnn base
   TUs fall back to SSE paths and its runtime dispatch still selects AVX/AVX2. Update the comment
   to describe both variants.
6. Add presets `qimgv-x64-release-sse2` (`QIMGV_ISA=SSE2`, `exiv2_DIR` pointing to
   `install-sse2`).

Implementation notes:

- ISA-dependent paths must be normal variables derived from `QIMGV_ISA`, never `CACHE` entries.
  A cache entry keeps the value of the first configure, so a later ISA change in the same build
  directory would still link the other ISA's dependency. The former `QIMGV_ZSTD_ROOT` cache
  entry was replaced by `ZSTD_INSTALL_DIR` in `qimgv/CMakeLists.txt`, which `utils/`, `kimg_exr`
  and the `zstd.dll` deploy share. Existing build directories keep a stale, unused
  `QIMGV_ZSTD_ROOT` cache entry; it has no effect.
- `exiv2_DIR` from the presets takes precedence over the `find_package` hint, so
  `qimgv/CMakeLists.txt` fails the configure when the exiv2 it found is not inside the current
  ISA's install tree.
- Use one build directory per ISA (one preset each). Changing `QIMGV_ISA` in an existing build
  directory is not a supported workflow.

Done when: an AVX2 configure produces a byte-for-byte equivalent command line to today's build,
and an SSE2 configure points every dependency path to `install-sse2` and outputs to
`release-sse2`.

Commit: `Add QIMGV_ISA build option with per-ISA dependency and output directories`

## Stage 2: ISA Parameter in Dependency Build Scripts

Status: [ ]

Files: `build_scripts/msvc-release-policy.json`, `build_scripts/MsvcPolicy.ps1`,
`build_scripts/rebuild-all.ps1`, `build_exiv2.ps1`, `build_qjpeg_jpeg.ps1`,
`build_qpng_spng.ps1`, `build_qtiff_jpeg.ps1`, `deploy-plugins.ps1`, `setup-deps.ps1`,
`build_ffmpeg_msvc.py`.

1. Move the ISA flag out of `compile` in `msvc-release-policy.json` into a map:
   `"isa": { "AVX2": "/arch:AVX2", "SSE2": "/arch:SSE2" }`.
2. `MsvcPolicy.ps1`: `Get-MsvcPolicyCMakeArgs -Isa <AVX2|SSE2>` (default `AVX2`) builds the flag
   string from `isa[Isa]` + `compile`. Also expose a helper that returns the install and build
   folder names for an ISA (`install` / `install-sse2`, `build` / `build-sse2`).
3. Every script gets `-Isa` (`ValidateSet("AVX2","SSE2")`, default `AVX2`) and uses the helper
   instead of literal `install` / `build` folder names.
4. `build_ffmpeg_msvc.py`: add `--isa` with the same values and read the flag from the same JSON
   map. The SSE2 prefix is `formats/ffmpeg/ffmpeg-build-msvc-sse2`.
5. LibRaw (nmake `buildfiles/release-x86_64`): the SSE2 build goes to
   `buildfiles/release-x86_64-sse2`, which `kimg_raw` and `shellex` already expect (Stage 1).
6. libjpeg-turbo and zlib-ng have their own runtime SIMD dispatch. Build them with the plain SSE2
   baseline and keep their SIMD options enabled.

Stage 1 derives every ISA-specific folder from `QIMGV_DEPS_SUFFIX` (empty for AVX2, `-sse2` for
SSE2): `install<suffix>`, `build<suffix>` (libavif, libjxl), `release-x86_64<suffix>` (LibRaw),
`ffmpeg-build-msvc<suffix>`, and `build_scripts/{qtiff_jpeg,qjpeg_jpeg}/install<suffix>`. The
scripts must produce exactly these names.

Implementation notes:

- Each `isa` entry in the policy JSON is an object, `{ "arch": "/arch:SSE2", "deps_suffix": "-sse2" }`,
  so `build_ffmpeg_msvc.py` and the PowerShell scripts share both the flag and the folder
  suffix. `deps_suffix` must stay equal to `QIMGV_DEPS_SUFFIX` in `cmake/QimgvMsvcPolicy.cmake`.
  The `arch` flag is placed before `compile`, so the AVX2 command lines are unchanged.
- `MsvcPolicy.ps1` exposes `Get-MsvcPolicyCMakeArgs -Isa`, `Get-MsvcPolicyCompileFlags -Isa`
  (LibRaw nmake) and `Get-IsaFolderName -BaseName -Isa`. Intermediate build folders get the
  suffix too (`build_msvc-sse2`, `build-msvc-sse2`, `qtiff_jpeg/build-sse2`), so both ISAs can
  stay configured side by side.
- `build_qtiff_jpeg.ps1` and `build_qjpeg_jpeg.ps1` deploy into the shared Qt SDK only for the
  default ISA (`$MSVC_DEFAULT_ISA`). With `-Isa SSE2` they update only `release-sse2`.
- openjpeg is now the `openjpeg` step of `rebuild-all.ps1`, part of the default list. It uses
  the hand-built configuration (static, no codec/JPIP/tests/docs, IPO) plus the shared policy.
  The previous hand-built AVX2 `install` lacked the hardening flags; rebuild it once.
- LibRaw's nmake build is in-tree. Each run cleans first, and only the copy into
  `buildfiles/release-x86_64<suffix>` keeps the ISAs apart. FFmpeg is also built in-tree
  (`make clean` first), so its prefix is the only per-ISA output.
- libavif fetches its LOCAL codecs (dav1d, aom, libyuv) into the build folder, so the first
  `-Isa SSE2` run downloads them again into `build-sse2`.
- `deploy-plugins.ps1 -Isa SSE2` reads `out/build/qimgv-x64-release-sse2` and writes
  `release-sse2`.
- `.gitignore` covers `release-sse2/`, `build-sse2/` and `install-sse2/`.

Done when: `rebuild-all.ps1` without `-Isa` behaves exactly as before (plus the new openjpeg
step), and `-Isa SSE2` fills only `install-sse2` / `build-sse2` folders.

Commit: `Add ISA parameter to dependency build scripts`

## Stage 3: HDR Tone Mapper Fallback

Status: [ ]

File: `qimgv/utils/hdrtonemapper.cpp`.

1. Keep the `__m256` primitives and AVX2 worker loop inside `#if defined(__AVX2__)`.
2. In `#else`, process whole rows with the existing `*Scalar` helpers (the code that already
   handles row tails). Do not duplicate the math. Restructure so AVX2 and SSE2 share the scalar
   helpers.
3. Guard `#include <immintrin.h>` usage accordingly. SSE2 intrinsics are not required in this
   stage; MSVC auto-vectorizes simple scalar loops with SSE2.
4. Verify visually: the same HDR/EXR/AVIF-PQ files on both builds. Small rounding differences
   are expected, because FMA in AVX2 is not used in the scalar path.

Implementation notes:

- The per-pixel loop in `ToneMapTask::run()` was already the AVX2 build's path for
  premultiplied, 8-bit and narrow (< 8 px) rows. Only `<immintrin.h>`, the `__m256` primitives
  and the vectorized row branch are guarded. Non-AVX2 builds send every row through that loop,
  so no math is duplicated. The AVX2 build's vectorized path is unchanged.
- Fixed while auditing that loop: the premultiplied FP16 branch divided by alpha and then
  discarded the result (the LUT lookup re-read the original half bits). It now un-premultiplies,
  clamps and re-quantizes to half before the lookup, like the FP32 branch.
- Known scalar/AVX2 difference (unchanged): for FP32 input the AVX2 path converts to half
  without clamping, so values above 65504 become `inf` and map to 0 (black) through the LUT.
  The scalar path clamps them to 65504. This only affects extreme linear values.

Commit: `Add scalar fallback for HDR tone mapping in non-AVX2 builds`

## Stage 4: Image Scaling and Color Adjustment Fallback

Status: [ ]

File: `qimgv/utils/imagelib.cpp`.

Scaler coverage (`ImageLib::scaled` dispatch, line ~542):

| Filter | Implementation | Stage 4 work |
|---|---|---|
| Nearest, Bilinear | `scaled_Qt` -> `QImage::scaled` | None. Qt dispatches its SIMD at runtime. |
| Smart | `scaled_Smart`, four AVX2 passes | Yes (step 3). |
| MKS2021 | `scaled_MKS2021`, four AVX2 passes | Yes (steps 1-2). |
| Upscayl (ncnn) | `upscayl-ncnn.dll`, Vulkan | None. ncnn dispatches CPU code at runtime (Stage 7 allowlist). |

1. Add SSE2-only per-pixel helpers (SSE2 is the x64 baseline) for the MKS horizontal and
   vertical taps and for the `scaled_Smart` passes. The existing "Scalar/SSE tails" use
   SSE4.1 and FMA3 (audit correction 1) and must not be reused in the `#else` path. The AVX2
   builds use the same helpers for their odd trailing pixel.
2. MKS2021: guard the vector loops of `mksHorizontalFixed`, `mksVerticalFixed`,
   `mksHorizontalGeneral` and `mksVerticalGeneral` with `#if defined(__AVX2__)`. In `#else`, the
   scalar helpers process every pixel. `scaled_MKS2021` (line ~1137) keeps its fixed/general
   selection in one place.
3. `scaled_Smart`: guard all four AVX2 passes (audit correction 2): upscale horizontal, upscale
   vertical, sharpen and downscale Gaussian. In `#else`, the existing scalar loops (upscale,
   sharpen "Scalar fallback") or the new Gaussian helper cover the full range.
4. `applyColorAdjustments` (~1328): the AVX2 loop goes under `#if defined(__AVX2__)`, and the
   existing "Scalar fallback" loop covers the full range in `#else`.
5. Keep the per-pixel code in shared helpers instead of copying it into the `#else` branches.

Implementation notes:

- Helpers `unpackPixelSse2`, `accumulatePixelSse2` and `packPixelSse2` hold one pixel as four
  float lanes. SSE4.1 `cvtepu8_epi32` is replaced by two SSE2 unpacks, SSE4.1 `packus_epi32` by
  SSE2 `packs_epi32` (lanes are already clamped to [0, 255]), and FMA by `mul` + `add`. The
  rounding matches the AVX2 loops (add 0.5, clamp, `cvtps2dq`).
- Each former `if (x < W)` tail is now a `for (; x < W; ++x)` loop after the guarded AVX2
  loop. It handles the odd trailing pixel in AVX2 builds and the whole row in SSE2 builds, so
  no per-pixel math is duplicated.
- The `scaled_Smart` upscale tails previously used plain C++ with truncation. They now use the
  SSE2 helpers, so the AVX2 build's last odd column rounds like the rest of the row.
- `applyColorAdjustments`: the AVX2 constants moved into the task lambda under the guard, so
  the capture list no longer depends on the ISA. The sharpen pass and the color loop keep
  their existing scalar loops.
- Known AVX2/SSE2 difference: `applyColorAdjustments` scalar loop rounds with
  `int(v + 0.5)`. In SSE2 builds every pixel goes through it, so it can differ by 1 from the
  AVX2 build (see the rounding issue below).

Found during this stage (not fixed here; separate commits):

- **Rounding bias in all AVX2 resample/color loops and the SSE2 helpers.** They add
  `kRoundOffset` (0.5) and then convert with `cvtps2dq`, which already rounds to nearest. The
  result is rounded up twice: 10.3 becomes 11, and an exact 11.0 becomes 12. Fix: drop the
  `+ 0.5` before `cvtps2dq` (or use truncating `cvttps2dq`) in all paths together, so AVX2
  and SSE2 builds stay identical.
- **Out-of-bounds read in the `scaled_Smart` sharpen pass when `W_dst == 1`.** The `x = 0`
  border reads `rowC[1]` from a one-element buffer. This happens when a 1-pixel-wide image is
  upscaled vertically. Fix: use `c` for the right neighbor when `W_dst == 1`.

Commit: `Add scalar fallback for MKS scaling and color adjustments in non-AVX2 builds`

## Stage 5: Radiance HDR Plugin Fallback

Status: [ ]

File: `qimgv/3rdparty/kimageformats/hdr.cpp`.

1. Guard `storeRgbePairAvx2` and the gather-based block loop with `#if defined(__AVX2__)`.
2. In `#else`, use the per-pixel RGBE conversion (existing scalar path, or add one based on
   Stage 0 findings).

Commit: `Add scalar RGBE decode path for non-AVX2 builds`

## Stage 6: Installer Variants

Status: [ ]

File: `qimgv/qimgv-plus.iss`.

1. Add an ISPP define with a default: `#ifndef Isa` / `#define Isa "AVX2"` / `#endif`.
   Build the SSE2 installer with `iscc /DIsa=SSE2 qimgv-plus.iss`.
2. Per variant:
   - `OutputBaseFilename`: `qimgv-plus-setup` / `qimgv-plus-setup-sse2`.
   - `[Files]` source folder: `..\release` / `..\release-sse2` (one `#define` used by all
     `Source:` lines).
   - `InitializeSetup`: the AVX2 variant keeps the AVX2 check. Update `AVX2ErrorMsg` in all
     languages to point to the SSE2 installer. The SSE2 variant has no CPU check.
3. Keep the same `AppId`, so both variants install to the same location and replace each
   other.

Commit: `Build AVX2 and SSE2 installer variants from one script`

## Stage 7: ISA Verification

Status: [ ]

1. Add `build_scripts/check-isa.ps1 -Path <release dir>`:
   - Run `dumpbin /disasm` on every `*.exe` / `*.dll`.
   - Report files that contain `ymm` registers or VEX-encoded (`v`-prefixed) instructions.
   - Allowlist files with confirmed runtime dispatch (ncnn inside `upscayl-ncnn.dll`,
     libjpeg-turbo, zlib-ng). Their AVX code is reachable only after a CPUID check.
   - Exit with a non-zero code on any unexpected file.
2. Smoke test the SSE2 build under the Intel Software Development Emulator on a pre-AVX CPU
   model: `sde -mrm -- release-sse2\qimgv-plus.exe`. Open at least JPEG, PNG, TIFF, HEIC, AVIF,
   JXL, JXR, EXR, Radiance HDR, a RAW file, DjVu and an archive. Run upscaling, color
   adjustments and MKS scaling. Any illegal-instruction fault fails the release.

Commit: `Add ISA verification script for release folders`

## Stage 8: Release Procedure

Status: [ ]

Document the steps in `BUILDING.md`:

1. Dependencies: `rebuild-all.ps1` and the other scripts, once without `-Isa` and once with
   `-Isa SSE2`. Rebuild them only when a dependency changes.
2. Application: build presets `qimgv-x64-release` and `qimgv-x64-release-sse2`.
3. Verification: `check-isa.ps1` on both release folders, plus the SDE smoke test on
   `release-sse2`.
4. Installers: `iscc qimgv-plus.iss` and `iscc /DIsa=SSE2 qimgv-plus.iss`.

Commit: `Document the dual AVX2/SSE2 release procedure`

## Maintenance Rule

Any change to a function inside an `#if defined(__AVX2__)` block must also be applied to its
`#else` branch in the same commit. Changes to the scalar helpers apply to both builds
automatically.
