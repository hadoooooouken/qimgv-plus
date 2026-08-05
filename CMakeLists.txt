# qimgv root
cmake_minimum_required(VERSION 3.25)
project(qimgv-plus
    VERSION 3.4.3.0
    HOMEPAGE_URL "https://github.com/hadoooooouken/qimgv-plus"
    LANGUAGES C CXX RC)

message(STATUS "Build configuration: " ${CMAKE_BUILD_TYPE})

# AUTOMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# OPTIMIZATIONS & HARDENING
if(MSVC)
    # Compiler options: AVX2, Buffer Security Check (/GS), Control Flow Guard (/guard:cf), C++ Exception Unwind (/EHsc), Spectre mitigations (/Qspectre)
    # NOTE: /arch:AVX2 is now added to CMAKE_C_FLAGS as well, not just CMAKE_CXX_FLAGS.
    # Without it, libdeflate (and any other plain-C code) gets built by MSVC without
    # any AVX2/AVX-512 code paths at all: unlike GCC/Clang, MSVC has no per-function
    # target attributes, so a SIMD path only ends up in the binary if the ENTIRE
    # translation unit is compiled with /arch:AVX2.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2 /GS /guard:cf /EHsc /Qspectre")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:AVX2 /GS /guard:cf /Qspectre")

    # Linker options: Control Flow Guard (/guard:cf), ASLR (/DYNAMICBASE), 64-bit ASLR (/HIGHENTROPYVA), DEP (/NXCOMPAT), CET Shadow Stack (/CETCOMPAT)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /guard:cf /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /CETCOMPAT")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /guard:cf /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /CETCOMPAT")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /guard:cf /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /CETCOMPAT")

    # Clean Release flags to perfectly map to Visual Studio UI properties
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /Oi /Ot /DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "/MD /O2 /Ob2 /Oi /Ot /DNDEBUG")

    # Enable Whole Program Optimization (/GL) and Link Time Code Generation (/LTCG) natively in MSBuild/VS
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/wd4005)
    message(STATUS "AVX2, full optimizations (/O2 /Ob2 /Oi /Ot), security hardening (/GS /guard:cf /DYNAMICBASE /HIGHENTROPYVA), CRT and C4005 warnings suppression and LTCG/GL enabled for Release")
endif()

# USER OPTIONS
option(EXIV2 "For reading/writing exif tags" ON)

# FIND PACKAGES
set(QT_VERSION_MAJOR 6)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Svg SvgWidgets PrintSupport OpenGLWidgets Sql Pdf)


if(EXIV2)
    # Try to find exiv2 in standard paths first, fallback to the local build
    find_package(exiv2 QUIET)
    if(NOT exiv2_FOUND)
        # Hint to find the local exiv2 build in formats/exiv2
        list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/formats/exiv2")
        find_package(exiv2 REQUIRED)
    endif()
endif()


##############################################################

# --- UPSCAYL NCNN INTEGRATION ---

if(DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK_DEFAULT "$ENV{VULKAN_SDK}")
else()
    set(VULKAN_SDK_DEFAULT "E:/VULKAN")
endif()

set(VULKAN_SDK_PATH "${VULKAN_SDK_DEFAULT}" CACHE PATH "Path to Vulkan SDK")

if(NOT EXISTS "${VULKAN_SDK_PATH}")
    message(FATAL_ERROR "Vulkan SDK not found: ${VULKAN_SDK_PATH}")
endif()

message(STATUS "Configuring Upscayl NCNN with Vulkan SDK: ${VULKAN_SDK_PATH}")

list(APPEND CMAKE_PREFIX_PATH "${VULKAN_SDK_PATH}")

if(NOT DEFINED ENV{VULKAN_SDK})
    set(ENV{VULKAN_SDK} "${VULKAN_SDK_PATH}")
endif()

set(NCNN_VULKAN ON CACHE BOOL "Build NCNN with Vulkan support")
set(NCNN_VULKAN_ONLINE_SPIRV ON CACHE BOOL "Build NCNN with Vulkan online SPIRV compilation support")

add_subdirectory(upscayl-ncnn/src)

add_subdirectory(qimgv)
add_subdirectory(shellex)
