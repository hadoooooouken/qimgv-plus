# qimgv root
cmake_minimum_required(VERSION 3.25)
project(qimgv-plus
    VERSION 3.6.3.0
    HOMEPAGE_URL "https://github.com/hadoooooouken/qimgv-plus"
    LANGUAGES C CXX RC)

message(STATUS "Build configuration: " ${CMAKE_BUILD_TYPE})

# AUTOMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# OPTIMIZATIONS & HARDENING
# ISA, IPO, hardening and CRT policy is applied per target through
# qimgv_apply_msvc_profile() (cmake/QimgvMsvcPolicy.cmake). Only the Release
# optimization baseline is set here; it is shadowed as a normal variable so a
# stale cache entry (e.g. one carrying /MD) cannot leak into the build.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(QimgvMsvcPolicy)
if(MSVC)
    set(CMAKE_C_FLAGS_RELEASE "${QIMGV_MSVC_RELEASE_BASELINE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${QIMGV_MSVC_RELEASE_BASELINE_FLAGS}")
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

# upscayl-ncnn.dll is shipped by this project and gets the full profile.
# Its ncnn/glslang static libraries are LTCG IL (the upscayl subproject enables
# IPO for itself), so the DLL keeps Release IPO to link them without a restart.
qimgv_apply_msvc_profile(upscayl-ncnn)
qimgv_suppress_crt_deprecation(upscayl-ncnn)

# ncnn adds /arch:SSE2 to its base TUs and dispatches at runtime to TUs whose
# source flags end with /arch:AVX, /arch:AVX2 or /arch:AVX512; that strategy is
# left intact. The application baseline is still placed BEFORE ncnn's own
# /arch:SSE2 on purpose: MSVC lowers code generation to the last /arch, but
# keeps __AVX__/__AVX2__ predefined from the earlier /arch:AVX2, and ncnn's x86
# sources select their AVX/AVX2 intrinsic paths by those macros. Dropping it
# removed those paths from the base TUs (e.g. convolution_x86.cpp: 6691 ymm
# instructions -> 0), so this ordering reproduces the established build.
# glslang has no ISA strategy of its own and gets the regular AVX2 baseline.
# All of them keep the IPO setting of the upscayl subproject; /guard:ehcont on
# them is required for the upscayl-ncnn.dll link to accept /guard:ehcont.
target_compile_options(ncnn BEFORE PRIVATE "$<$<COMPILE_LANGUAGE:C,CXX>:${QIMGV_MSVC_ISA_OPTIONS}>")
qimgv_apply_msvc_profile(ncnn NO_ARCH NO_IPO)
foreach(_glslang_target glslang SPIRV MachineIndependent GenericCodeGen OGLCompiler OSDependent)
    if(TARGET ${_glslang_target})
        qimgv_apply_msvc_profile(${_glslang_target} NO_IPO)
    endif()
endforeach()

add_subdirectory(qimgv)
add_subdirectory(shellex)
