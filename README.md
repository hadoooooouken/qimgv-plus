qimgv-plus | Current version: 1.0.0
==========
A Windows-optimized fork of the [qimgv](https://github.com/easymodo/qimgv) image viewer, featuring GPU-accelerated scaling and native support for modern image formats.

## Key features:

- **Simple UI & Fast**: Lightweight and extremely responsive interface.

- **NVIDIA GPU-Accelerated Scaling (CUDA)**: Extremely smooth and high-quality zooming offloaded to your GPU, including an experimental "ULTRA" mode that filters digital noise and sharpens details in real-time.

- **Real-Time Color Adjustments**: Instant, lag-free adjustments of Brightness, Contrast, Saturation, and Hue calculated on the graphics card via shaders. Accessible via the right-click context menu while viewing an image.

- **360-Degree Spherical Panorama View**: Interactive drag-and-look viewer for spherical panoramic photos.

- **Native Support for Modern Formats**: Out-of-the-box support for AVIF, HEIF/HEIC, JPEG XL (JXL), PSD, DDS, RAW, KRA, and ORA.

- **Modern CPU Optimization**: Compiled with AVX2 support for high-speed image processing on modern processors.

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
| Exit fullscreen mode | Esc |
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
| Folder view | Enter / Backspace |
| Open | Ctrl+O |
| Print / Export PDF | Ctrl+P |
| Settings  | P |
| Exit application | Esc / Ctrl+Q / Alt+X / MiddleClick |
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

qimgv-plus supports high-quality, hardware-accelerated scaling filters:
- **GPU-Accelerated Scaling (NVIDIA CUDA)**: Offloads scaling to your NVIDIA graphics card using CUDA/NPP (when built with CUDA support). Filter options in __Settings > Scaling__ include **Bicubic (CUDA)**, **Lanczos (CUDA)**, and the experimental **ULTRA (CUDA)** mode (filters digital noise and applies smart sharpening in real-time).
- **CPU Scaling**: Standard high-quality CPU scaling filters (**Bicubic** or **Bilinear+Sharpen**) are available when built with OpenCV support (enabled by default).

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
