# qimgv-plus 🚀

![Version](https://img.shields.io/github/v/release/hadoooooouken/qimgv-plus?color=blue&label=Version) ![License](https://img.shields.io/github/license/hadoooooouken/qimgv-plus?color=green) ![Platform](https://img.shields.io/badge/Platform-Windows-0078D4?logo=windows&logoColor=white) ![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white) ![Qt](https://img.shields.io/badge/Qt-5%2F6-41CD52?logo=qt&logoColor=white) ![CPU](https://img.shields.io/badge/CPU-AVX2-orange) ![GPU](https://img.shields.io/badge/GPU-OpenGL-0696D7?logo=opengl&logoColor=white) ![Vulkan](https://img.shields.io/badge/Vulkan-NCNN-E6522C?logo=vulkan&logoColor=white) ![AI Upscaling](https://img.shields.io/badge/AI_Upscaling-Upscayl-8A2BE2)
A Windows-optimized fork of the [qimgv](https://github.com/easymodo/qimgv) image viewer, featuring high-quality CPU-accelerated scaling and native support for modern image formats.

## Key features:

- **Simple UI & Fast**: Lightweight and extremely responsive interface.

- **GPU-Accelerated Sharpening (FidelityFX CAS)**: High-quality image scaling and sharpening based on AMD's Contrast Adaptive Sharpening (CAS). Processed entirely on the GPU via OpenGL shaders with zero CPU overhead. Features an interactive overlay settings panel for real-time sharpening and contrast adjustments.

- **Smart Sharpen Scaling (CPU)**: High-quality, fast scaling performed on the CPU. The "Smart sharpen" mode combines custom Separable 1D Bicubic upscaling with a custom cross-kernel sharpening pass, and Bilinear downscaling with an anti-aliasing Gaussian unsharp mask, ensuring maximum texture detail without jagged edges.

- **Real-Time AI Upscaling (Upscayl)**: Real-time AI image upscaling powered by **[upscayl-ncnn](https://github.com/upscayl/upscayl-ncnn) (NCNN/Vulkan & RealESRGAN)**. Upscales only the visible viewport crop to conserve VRAM, using background threads to keep the UI fully responsive. Includes an optional preloading mechanism to warm up Vulkan shader pipelines and eliminate startup latency.

- **Real-Time Color Adjustments**: Instant, lag-free adjustments of Brightness, Contrast, Saturation, Hue, Exposure, Temperature, and Tint calculated on the graphics card via shaders. Features an interactive overlay with a "Compare" button (press and hold to view the original) and double-click slider resets.

- **360-Degree Spherical Panorama View**: Interactive drag-and-look viewer for spherical panoramic photos, featuring exposure, temperature, and tint adjustments.

- **Native Support for Modern Formats**: Out-of-the-box support for AVIF, HEIF/HEIC, JPEG XL (JXL), JPEG 2000 (.jp2, .j2k, .jpf, .jpx, .jpc), HTJ2K (.jph), PSD, DDS, RAW, KRA, ORA, and QOI.

- **Modern CPU Optimization**: Compiled with AVX2 support for high-speed image processing on modern processors.

- **Batch Image Converter**: Multi-threaded background queue to convert, resize, rename, adjust colors, and AI-upscale multiple images.

- **Display Color Management**: Dynamic source-to-target color mapping that queries the system's primary monitor ICC profile. Supports standard preset spaces (sRGB, Adobe RGB, Display P3, etc.) or custom ICC/ICM files.

- **Fully Configurable**: Highly customizable keyboard shortcuts, theme modes (System Auto, Dark, Light), custom accent colors, and background preferences.

- **Basic Image Editing**: Quick crop, rotate, and resize operations.

- **Folder View Mode**: Seamless grid browsing with support for left mouse button rectangular selection and options to open only selected files.

- **Quick Copy / Move**: Easily categorize or organize your files into configurable folders.

- **Open with**: Launch the current image in an external application or custom command.

- **Multilingual UI**: Interface available in 8 languages — English, German, French, Spanish, Ukrainian, Japanese, Turkish, and Simplified Chinese.

## Default control scheme:

| Action  | Shortcut |
| ------------- | ------------- |
| Next image  | Right arrow / MouseWheel |
| Previous image  | Left arrow / MouseWheel |
| Goto first image  | Home |
| Goto last image  | End |
| Zoom in  | Ctrl+MouseWheel / Ctrl+Up |
| Zoom out  | Ctrl+MouseWheel / Ctrl+Down |
| Zoom (alt. method) | Hold right mouse button & move up / down |
| Next / Previous image (gesture) | Hold right mouse button & move left / right |
| Fit mode: window | 1 |
| Fit mode: width | 2 |
| Fit mode: 1:1 (no scaling) | 3 |
| Fit mode: window stretch | 4 |
| Switch fit modes  | Space |
| Toggle fullscreen mode  | DoubleClick / F / F11 |
| Show EXIF panel  | I |
| Crop image  | X |
| Resize image  | R |
| Flip horizontally | H |
| Flip vertically | V |
| Rotate left  | Ctrl+L |
| Rotate Right  | Ctrl+R |
| Open containing directory | Ctrl+D |
| Slideshow mode | ~ |
| Shuffle mode | Ctrl+~ |
| Quick copy  | C |
| Quick move  | M |
| Move to trash | Delete |
| Delete file  | Shift+Delete |
| Save  | Ctrl+S |
| Save As  | Ctrl+Shift+S |
| Discard edits | Ctrl+Z |
| Copy image to clipboard | Ctrl+C |
| Copy viewport to clipboard | Shift+C |
| Copy path to clipboard | Ctrl+Shift+C |
| Rename file | F2 |
| Reload image | F5 |
| Next directory | Shift+Right |
| Previous directory | Shift+Left |
| Folder view | Backspace / Esc / MiddleClick |
| Open | Ctrl+O |
| Print / Export PDF | Ctrl+P |
| Toggle scaling filter (nearest / configured) | N |
| Toggle Use Upscayl | Alt+I |
| Settings  | P |
| Exit application | Ctrl+Q / Alt+X |
| Toggle panorama mode | Shift+P |
| Panorama: Look around | Click & drag mouse (360° view) |
| Panorama: Zoom | MouseWheel (360° view) |

... and more.

Note: you can configure every shortcut by going to **Settings > Controls**

# User interface

The idea is to have a uncluttered, simple and easy to use UI. You can see UI elements only when you need them.

There is a pull-down panel with thumbnails, as well as folder view. You can also bring up a context menu via right click.

## Using quick copy / quick move panels

Bring up the panel with C or M shortcut. You will see 9 destination directories, click on the folder icon to change them.

With panel visible, use 1 - 9 keys to copy/move current image to corresponding directory.

When you are done press C or M again to hide the panel.

## Open with

You can open the current image in external applications or run custom commands on it.

Open **Settings > Scripts**. Press Add. Here you can choose between a command and a script file. 

Example of a command (using `%file%` as a placeholder for the image path): 

`magick %file% %file%_.pdf`

Example of a Windows batch script (`.bat` or `.cmd`, where `%1` is the image path): 
```batch
@echo off
"C:\Program Files\GIMP 2\bin\gimp-2.10.exe" "%1"
```
_Note: On Windows, you can use standard batch files (`.bat` / `.cmd`) directly. For PowerShell (`.ps1`) or Python (`.py`) scripts, you should configure them as a **command** (e.g. `powershell -File C:\script.ps1 %file%`)._

When you've created your script go to **Settings > Controls > Add**, then select it and assign a shortcut like for any regular action.

## Theme & UI Customization

qimgv-plus offers modern options to personalize your image viewing experience:

- **Theme Mode Selector**: Under **Settings > Theme**, switch between:
  - *System Default (Auto)*: Follows Windows system light or dark preferences.
  - *Dark*: Forces a dark appearance.
  - *Light*: Forces a light appearance.
- **Custom Accent Color**: Choose a custom accent color under settings to match your personal setup.
- **Black Background Option**: Enable "Use black for background and thumbnail bar" to override the theme colors and keep the focus entirely on the image viewport.

## Scaling and Sharpening Filters

qimgv-plus features advanced CPU and GPU scaling systems:

- **FidelityFX Contrast Adaptive Sharpening (CAS)**: A high-quality GPU-accelerated scaling and sharpening filter based on AMD's Contrast Adaptive Sharpening. It runs entirely via OpenGL shaders with zero CPU overhead.
  - *Interactive CAS Panel*: A floating overlay for real-time adjustments of sharpening and contrast parameters. Open it via the context menu. Double-click sliders to quickly reset them to default values.
  - *Apply at 100% Scale*: Enable "Also use filter for 100% scale" under settings to apply the CAS filter even when viewing images at their native 1:1 resolution.
  - *Improved Downscaling*: Features automatic mipmap generation and trilinear filtering in OpenGL mode (active with CAS or color adjustments), which eliminates aliasing ("staircasing" artifacts) when zooming out.
- **Smart sharpen** (CPU): A high-performance scaling filter optimized with AVX2. For upscaling, it combines custom Separable 1D Bicubic interpolation with a custom cross-kernel sharpening pass. For downscaling, it combines Bilinear interpolation with an anti-aliasing Gaussian unsharp mask.
- **Standard CPU Filters**: Includes Nearest and Bilinear scaling.

## Real-Time AI Upscaling

qimgv-plus integrates **[upscayl-ncnn](https://github.com/upscayl/upscayl-ncnn) (NCNN/Vulkan & RealESRGAN)** for real-time AI image upscaling:
- **Viewport Crop Upscaling**: Conserves GPU resources and VRAM by upscaling only the currently visible region of the image.
- **Vulkan GPU Acceleration**: Heavy lifting is performed on background GPU threads, ensuring the main user interface remains smooth and interactive.
- **Model Options**: 
  * `4xLSDIRCompactC3` (Default): Included by default. Optimized for JPEG artifacts and general image restoration.
  * **Custom Models Support**: The application dynamically scans the `models/` directory. You can easily add more compatible models (e.g. from the [Upscayl Custom Models repository](https://github.com/upscayl/custom-models/tree/main/models)) by placing their `.param` and `.bin` files into the `models/` folder, and they will be automatically detected and made available in the settings menu.
- **VRAM Safety & Limits**: Auto-tile detection and input limits prevent out-of-memory errors on a wide range of GPUs.
- **Vulkan Preloading**: An optional pre-warm feature processes a dummy texture at startup to pre-allocate memory and eliminate initial rendering lag.
- **Hotkey**: Press **`Alt + I`** to instantly toggle AI upscaling.

## Batch Image Converter

Multi-threaded background queue processor:

- **Interactive Selection**: Includes filename, thumbnail, original format, and size info with checkboxes to filter the queue. Access this by selecting multiple files in Folder View and clicking **Batch convert ⇄**.
- **Output Options**: Target path selection, filename pattern templates (e.g., `{name}`), optional timestamped subdirectories (`Batch_YYYY-MM-DD_HH-MM-SS`), and file overwrite settings.
- **Formats**: Converts to **JPEG**, **PNG** (with 0-9 compression level slider), **WebP**, **JPEG-XL (JXL)**, **AVIF**, **BMP**, **TIFF**, and **QOI** with custom quality sliders.
- **Resizing**: Scaling by percentage or custom resolution, presets, aspect ratio lock, and quick enforcers ("Fit Desktop" / "Fill Desktop").
- **Scaling Filters**: Supports standard filters (Nearest, Bilinear, Smart Sharpen) and **Vulkan-accelerated AI Upscaling (Upscayl)** using customizable models (e.g. `4xLSDIRCompactC3`).
- **Color Correction**: Applies Exposure, Contrast, Brightness, Saturation, Hue, Temperature, and Tint adjustments. Includes a one-click reset button to clear all sliders.

## Display Color Management

Color space translation between source image and active display:

- **Activation**: Toggle via **Settings > View > Color Management**.
- **Display Profiles**: 
  * **System / Auto (Recommended)**: Queries the primary monitor's system ICC profile.
  * **Preset spaces**: `sRGB`, `Display P3`, `Adobe RGB`, `Rec. 2020`, `ProPhoto RGB`, and `Linear sRGB`.
  * **Custom target**: Browse and load custom `.icc` or `.icm` files from disk.
- **Mapping**: Reads embedded source ICC profile (assumes standard `sRGB` if missing) and converts it to the display space.

# Supported Image Formats

qimgv-plus natively supports standard formats as well as modern and professional image files out of the box (without requiring external plugin installations):

- **Modern formats**: JPEG XL (JXL), AVIF, HEIF / HEIC, JPEG XR (JXR / HDP), JPEG 2000 (.jp2, .j2k, .jpf, .jpx, .jpc), HTJ2K (.jph).
- **Game Textures & Formats**: DDS (DirectDraw Surface), TGA (Targa), QOI (Quite OK Image Format).
- **Digital Art & Projects**: PSD (Photoshop), AI (Adobe Illustrator), PDF, KRA (Krita), ORA (OpenRaster).
- **Professional & HDR**: OpenEXR (EXR), Radiance HDR (HDR).
- **Camera RAW**: RAW (including CR2, NEF, ARW, DNG, RAF, etc. powered by LibRaw).
- **Web & Standard formats**: WebP, APNG, ICO (Icons with smart size-matching frame selection), JPEG, PNG, GIF, BMP, SVG.

# Installation

qimgv-plus is optimized exclusively for **Windows (64-bit)** to deliver maximum performance.

- **Installer (.exe)**: Run the installer to automatically set up the application, configure file associations, and create shortcuts.
- **Portable Version**: Download the portable ZIP archive to run the application from any folder (all configuration is stored in the app folder).

Grab the latest version from the [releases page](https://github.com/hadoooooouken/qimgv-plus/releases).

# Donate

If you wish to give me a few bucks, please consider donating to the Ukrainian Army instead:

[https://savelife.in.ua/en/donate-en/#donate-army-card-once](https://savelife.in.ua/en/donate-en/#donate-army-card-once)

[https://u24.gov.ua/](https://u24.gov.ua/)

This directly increases the chances of me being able to work on this in future
