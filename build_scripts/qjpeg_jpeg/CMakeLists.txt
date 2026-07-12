# Copyright (c) 2025 qimgv project
# SPDX-License-Identifier: MIT
#
# Standalone build for qjpeg.dll (Qt JPEG image format plugin)
# linked against the shared, AVX2/LTCG-optimized libjpeg-turbo built by
# build_qtiff_jpeg.ps1 (Step 2: libjpeg-turbo, shared).
#
# This is the JPEG counterpart of qtiff_jpeg/CMakeLists.txt: it compiles
# the Qt JPEG handler source files straight from the Qt SDK source tree
# and links them against our own jpeg.lib/jpeg*.dll instead of Qt's
# bundled (older, non-AVX2) libjpeg copy. The result is a drop-in
# replacement for the stock qjpeg.dll, and it shares its JPEG codec with
# qtiff.dll (via libtiff) instead of carrying a second copy.

cmake_minimum_required(VERSION 3.16)
project(qjpeg-turbo LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ---------------------------------------------------------------------------
# Qt SDK source paths (adjust if your Qt source location differs)
# ---------------------------------------------------------------------------
# Unlike the TIFF plugin, the stock JPEG plugin lives in qtbase (not
# qtimageformats) -- see qtbase/src/plugins/imageformats/jpeg.
set(QT_JPEG_HANDLER_SRC_DIR "E:/Qt/6.11.1/Src/qtbase/src/plugins/imageformats/jpeg"
    CACHE PATH "Path to Qt JPEG handler source directory")

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

# Qt6 Core/Gui (+ their Private variants -- qjpeghandler.cpp uses private
# Qt headers, same as upstream qtbase/src/plugins/imageformats/jpeg/CMakeLists.txt)
#
# NOTE: Qt6::CorePrivate / Qt6::GuiPrivate are NOT created automatically just
# by finding "Core"/"Gui" -- they must be requested explicitly as their own
# components, otherwise target_link_libraries fails with "target not found"
# even though the private headers are actually installed in the SDK.
find_package(Qt6 REQUIRED COMPONENTS Core Gui CorePrivate GuiPrivate)

# Our shared libjpeg-turbo (built by build_qtiff_jpeg.ps1, Step 2).
# The consumer must set CMAKE_PREFIX_PATH to point at its install dir so
# CMake's built-in FindJPEG module can locate jpeglib.h + jpeg.lib.
find_package(JPEG REQUIRED)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------

# Build as a shared library (MODULE = loadable plugin)
add_library(qjpeg MODULE
    "${QT_JPEG_HANDLER_SRC_DIR}/main.cpp"
    "${QT_JPEG_HANDLER_SRC_DIR}/qjpeghandler.cpp"
    "${QT_JPEG_HANDLER_SRC_DIR}/qjpeghandler_p.h"
)

# The handler source includes "jpeglib.h" (via qjpeghandler_p.h) -- provided
# by JPEG::JPEG below. Also include the handler's own directory so MOC can
# find jpeg.json (Q_PLUGIN_METADATA) and local headers.
target_include_directories(qjpeg PRIVATE
    "${QT_JPEG_HANDLER_SRC_DIR}"
)

# Link against Qt and our shared libjpeg-turbo import library. JPEG::JPEG
# here resolves to jpeg.lib (shared import lib), so the resulting qjpeg.dll
# has a runtime dependency on jpeg*.dll -- deploy it alongside (see
# Deploy-JpegRuntime in build_qtiff_jpeg.ps1 / build_qjpeg_jpeg.ps1, and
# root CMakeLists.txt).
target_link_libraries(qjpeg PRIVATE
    Qt6::Core
    Qt6::CorePrivate
    Qt6::Gui
    Qt6::GuiPrivate
    JPEG::JPEG
)

# Qt plugin output naming: qjpeg.dll (no "lib" prefix)
set_target_properties(qjpeg PROPERTIES
    OUTPUT_NAME "qjpeg"
    PREFIX ""
)

# Disable UNITY_BUILD (matches upstream Qt CMakeLists behavior)
set_target_properties(qjpeg PROPERTIES UNITY_BUILD OFF)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

install(TARGETS qjpeg
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)
