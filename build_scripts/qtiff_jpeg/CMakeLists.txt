# Copyright (c) 2025 qimgv project
# SPDX-License-Identifier: MIT
#
# Standalone build for qtiff.dll (Qt TIFF image format plugin)
# with JPEG compression support via statically linked libtiff + libjpeg-turbo + zlib.
#
# This project compiles the Qt TIFF handler source files from the Qt SDK source tree
# and links them against a custom libtiff that includes JPEG codec support.
# The result is a drop-in replacement for the stock qtiff.dll.

cmake_minimum_required(VERSION 3.16)
project(qtiff-jpeg LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ---------------------------------------------------------------------------
# Qt SDK source paths (adjust if your Qt source location differs)
# ---------------------------------------------------------------------------
set(QT_TIFF_HANDLER_SRC_DIR "E:/Qt/6.11.1/Src/qtimageformats/src/plugins/imageformats/tiff"
    CACHE PATH "Path to Qt TIFF handler source directory")
set(QT_BUNDLED_LIBTIFF_DIR  "E:/Qt/6.11.1/Src/qtimageformats/src/3rdparty/libtiff/libtiff"
    CACHE PATH "Path to Qt's bundled libtiff headers (used only for reference, NOT linked)")

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

# Qt6 Core and Gui are required for the image format plugin
find_package(Qt6 REQUIRED COMPONENTS Core Gui)

# Our custom libtiff with JPEG support (static library)
# The consumer must set CMAKE_PREFIX_PATH or tiff_DIR to point to the libtiff install.
#
# IMPORTANT: tiff-config.cmake does NOT call find_dependency() for static builds (upstream TODO).
# We must manually find ZLIB and JPEG before finding tiff so the targets exist when
# tiff-targets.cmake tries to reference them.
find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(tiff REQUIRED CONFIG)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------

# Build as a shared library (MODULE = loadable plugin)
add_library(qtiff MODULE
    "${QT_TIFF_HANDLER_SRC_DIR}/main.cpp"
    "${QT_TIFF_HANDLER_SRC_DIR}/qtiffhandler.cpp"
    "${QT_TIFF_HANDLER_SRC_DIR}/qtiffhandler_p.h"
)

# The handler source includes "tiffio.h" — provide our custom libtiff headers
# Also include the handler's own directory so MOC can find tiff.json and local headers
target_include_directories(qtiff PRIVATE
    "${QT_TIFF_HANDLER_SRC_DIR}"
)

# Link against Qt and our custom static libtiff (which pulls in JPEG + ZLIB transitively
# through INTERFACE_LINK_LIBRARIES in the installed CMake targets)
target_link_libraries(qtiff PRIVATE
    Qt6::Core
    Qt6::Gui
    TIFF::tiff
    JPEG::JPEG
    ZLIB::ZLIB
)

# Qt plugin output naming: qtiff.dll (no "lib" prefix)
set_target_properties(qtiff PROPERTIES
    OUTPUT_NAME "qtiff"
    PREFIX ""
)

# Disable UNITY_BUILD (matches upstream Qt CMakeLists behavior)
set_target_properties(qtiff PROPERTIES UNITY_BUILD OFF)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

install(TARGETS qtiff
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)
