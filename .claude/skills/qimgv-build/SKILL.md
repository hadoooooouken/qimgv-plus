---
name: qimgv-build
description: Configure, build, and run tests for qimgv with the repository's CMake presets and the local MSVC toolchain. Use when the user explicitly asks to build, compile, or configure qimgv in the current conversation, or when implementing or verifying an approved Qt Widgets-to-QML migration stage. Never use it merely because code was edited, fixed, or refactored.
---

# qimgv build

## Permission gate (check first)

Building is allowed only in one of these cases (see `AGENTS.md`, section 4):

1. The user directly asked to build, compile, or configure in the **current** conversation.
2. The work implements or verifies an **approved Qt Widgets-to-QML migration stage**. This standing authorization covers only the existing CMake configure, build, and test commands.

A request to implement, fix, refactor, or verify code is **not** a build request. If neither case applies, stop and tell the user the change is ready for their local build.

Never do any of the following without a separate explicit request:

- install or update dependencies (`setup-deps.ps1`, `rebuild-all.ps1`, `build_*.ps1`, `build_ffmpeg_msvc.py`)
- run `deploy-plugins.ps1`, packaging, or release steps
- make machine-wide changes (environment variables, SDK installs, registry)
- perform destructive cleanup: deleting `out/`, `build/`, or `release/`; `cmake --fresh`; removing `CMakeCache.txt`

## Toolchain

| Component | Location |
|-----------|----------|
| MSVC environment | `E:\MVSC\VC\Auxiliary\Build\vcvarsall.bat` (arch `x64`) |
| Ninja | Provided by the MSVC environment (`E:\MVSC\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja`) |
| Qt SDK | `E:\Qt\6.12.0\msvc2022_64` (set in `CMakePresets.json`) |
| Vulkan SDK | `E:\Vulkan` (set in `CMakePresets.json`) |

`cl.exe` and `ninja.exe` are available only after `vcvarsall.bat x64` runs, so every configure, build, or test command must run in the same `cmd` session as `vcvarsall.bat`. PowerShell runs in ConstrainedLanguage mode for agents, so do not try to import the vcvars environment into PowerShell. Run it through `cmd /c` instead.

## Presets

| Preset | Build type | Binary dir |
|--------|------------|------------|
| `qimgv-x64-release` | Release (default choice) | `out/build/qimgv-x64-release` |
| `qimgv-x64-relwithdebinfo` | RelWithDebInfo | `out/build/qimgv-x64-relwithdebinfo` |
| `qimgv-x64-debug` | Debug | `out/build/qimgv-x64-debug` |

Use `qimgv-x64-release` unless the user names another preset. `out/build/qimgv-x64-release` is the existing configured tree. The top-level `build/` directory is a stale, non-preset tree: do not use or delete it.

## Commands

Run from the repository root (`E:\qimgv`). Use a long timeout (up to 600000 ms) for builds, or `run_in_background` for full builds.

Configure only (needed when the binary dir is missing or `CMakeLists.txt` / presets changed; the build step re-runs configure automatically otherwise):

```powershell
cmd /c 'call "E:\MVSC\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul && cmake --preset qimgv-x64-release'
```

Build:

```powershell
cmd /c 'call "E:\MVSC\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul && cmake --build --preset qimgv-x64-release'
```

Build a single target (faster when verifying one component):

```powershell
cmd /c 'call "E:\MVSC\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul && cmake --build --preset qimgv-x64-release --target <target>'
```

List available targets:

```powershell
cmd /c 'call "E:\MVSC\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul && cmake --build --preset qimgv-x64-release --target help'
```

Tests: the source tree does not register CTest tests yet. Run tests only after a stage adds them (`enable_testing()` / `add_test` / `qt_add_test`):

```powershell
cmd /c 'call "E:\MVSC\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul && ctest --test-dir out/build/qimgv-x64-release --output-on-failure'
```

The built application, plugins, and DLLs are placed in `release/`.

## Reporting results

- On success, state the preset and targets that were built, and the warnings introduced by the change, if any.
- On failure, quote the first real compiler or linker error with its `file(line)` location, not just the tail of the log. Fix errors caused by the current change. For environment or dependency errors (missing Qt, Vulkan, `formats/*` libraries), report them and stop; do not install or rebuild dependencies.
- Never claim a build passed without running it in this conversation.
