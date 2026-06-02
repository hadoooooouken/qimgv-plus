qimgv-plus | Current version: 2.0.4
==========
A Windows-optimized fork of the [qimgv](https://github.com/easymodo/qimgv) image viewer, featuring high-quality CPU-accelerated scaling and native support for modern image formats.

## Key features:

- **Simple UI & Fast**: Lightweight and extremely responsive interface.

- **Smart Sharpen Scaling (OpenCV)**: High-quality, fast scaling performed on the CPU using OpenCV. The default "Smart sharpen" mode combines Bicubic upscaling with a custom cross-kernel sharpening pass, and Area downscaling with an anti-aliasing Gaussian unsharp mask, ensuring maximum texture detail without jagged edges.

- **Real-Time AI Upscaling (Upscayl)**: Real-time AI image upscaling powered by **[upscayl-ncnn](https://github.com/upscayl/upscayl-ncnn) (NCNN/Vulkan & RealESRGAN)**. Upscales only the visible viewport crop to conserve VRAM, using background threads to keep the UI fully responsive. Includes an optional preloading mechanism to warm up Vulkan shader pipelines and eliminate startup latency.

- **Real-Time Color Adjustments**: Instant, lag-free adjustments of Brightness, Contrast, Saturation, and Hue calculated on the graphics card via shaders. Accessible via the right-click context menu while viewing an image.

- **360-Degree Spherical Panorama View**: Interactive drag-and-look viewer for spherical panoramic photos.

- **Native Support for Modern Formats**: Out-of-the-box support for AVIF, HEIF/HEIC, JPEG XL (JXL), PSD, DDS, RAW, KRA, and ORA.

- **Modern CPU Optimization**: Compiled with AVX2 support for high-speed image processing on modern processors.

- **Batch Image Converter**: Multi-threaded background queue to convert, resize, rename, adjust colors, and AI-upscale multiple images.

- **Display Color Management**: Dynamic source-to-target color mapping supporting system monitor ICC profiles, standard spaces (sRGB, Adobe RGB, Display P3, etc.), or custom ICC/ICM files.

- **Fully Configurable**: Highly customizable keyboard shortcuts and themes.

- **Basic Image Editing**: Quick crop, rotate, and resize operations.

- **Folder View Mode**: Seamless grid browsing.

- **Quick Copy / Move**: Easily categorize or organize your files into configurable folders.

- **Script Integration**: Ability to run external shell scripts on the current image.

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

Note: you can configure every shortcut by going to __Settings > Controls__

# User interface

The idea is to have a uncluttered, simple and easy to use UI. You can see UI elements only when you need them.

There is a pull-down panel with thumbnails, as well as folder view. You can also bring up a context menu via right click.

## Using quick copy / quick move panels

Bring up the panel with C or M shortcut. You will see 9 destination directories, click on the folder icon to change them.

With panel visible, use 1 - 9 keys to copy/move current image to corresponding directory.

When you are done press C or M again to hide the panel.

## Running scripts

You can run custom commands or scripts on the current image.

Open __Settings > Scripts__. Press Add. Here you can choose between a command and a script file. 

Example of a command (using `%file%` as a placeholder for the image path): 

`magick %file% %file%_.pdf`

Example of a Windows batch script (`.bat` or `.cmd`, where `%1` is the image path): 
```batch
@echo off
"C:\Program Files\GIMP 2\bin\gimp-2.10.exe" "%1"
```
_Note: On Windows, you can use standard batch files (`.bat` / `.cmd`) directly. For PowerShell (`.ps1`) or Python (`.py`) scripts, you should configure them as a **command** (e.g. `powershell -File C:\script.ps1 %file%`)._

When you've created your script go to __Settings > Controls > Add__, then select it and assign a shortcut like for any regular action.

## High quality scaling

qimgv-plus supports high-quality, OpenCV-accelerated CPU scaling filters:
- **Smart sharpen (OpenCV)** (Default): The ultimate scaling mode. For upscaling, it uses Bicubic interpolation combined with a $0.25$ strength cross-kernel sharpening pass. For downscaling, it uses Area relation interpolation with a $0.15$ strength Gaussian unsharp mask to restore textures without introducing aliasing.
- **Standard Filters**: Custom high-quality options include Bicubic, Lanczos, Bilinear+sharpen, and Area scaling.

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

- **Interactive Selection**: Includes filename, thumbnail, original format, and size info with checkboxes to filter the queue.
- **Output Options**: Target path selection, filename pattern templates (e.g., `{name}`), optional timestamped subdirectories (`Batch_YYYY-MM-DD_HH-MM-SS`), and file overwrite settings.
- **Formats**: Converts to **JPEG**, **PNG**, **WebP**, **JPEG-XL (JXL)**, **AVIF**, **BMP**, and **TIFF** with custom compression or quality sliders.
- **Resizing**: Scaling by percentage or custom resolution, presets, aspect ratio lock, and quick enforcers ("Fit Desktop" / "Fill Desktop").
- **Scaling Filters**: Supports standard filters (Nearest, Bilinear, Bicubic, Lanczos, Area, OpenCV Smart Sharpen) and **Vulkan-accelerated AI Upscaling (Upscayl)** using customizable models (e.g. `4xLSDIRCompactC3`).
- **Color Correction**: Applies Exposure, Contrast, Brightness, Saturation, Hue, Temperature, and Tint adjustments.

## Display Color Management

Color space translation between source image and active display:

- **Activation**: Toggle via __Settings > View > Color Management__.
- **Display Profiles**: 
  * **System / Auto (Recommended)**: Dynamically queries the active monitor's system ICC profile (updates automatically when moving the window between screens).
  * **Preset spaces**: `sRGB`, `Display P3`, `Adobe RGB`, `Rec. 2020`, `ProPhoto RGB`, and `Linear sRGB`.
  * **Custom target**: Browse and load custom `.icc` or `.icm` files from disk.
- **Mapping**: Reads embedded source ICC profile (assumes standard `sRGB` if missing) and converts it to the display space.

# Supported Image Formats

qimgv-plus natively supports standard formats as well as modern and professional image files out of the box (without requiring external plugin installations):

- **Modern formats**: JPEG XL (JXL), AVIF, HEIF / HEIC, JPEG XR (JXR / HDP).
- **Game Textures**: DDS (DirectDraw Surface), TGA (Targa).
- **Digital Art & Projects**: PSD (Photoshop), AI (Adobe Illustrator), PDF, KRA (Krita), ORA (OpenRaster).
- **Professional & HDR**: OpenEXR (EXR), Radiance HDR (HDR).
- **Camera RAW**: RAW (including CR2, NEF, ARW, DNG, RAF, etc.).
- **Web & Standard formats**: WebP, APNG, ICO (Icons), JPEG, PNG, GIF, BMP, SVG.

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
