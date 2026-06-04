# qimgv root
cmake_minimum_required(VERSION 3.13)
project(qimgv-plus
    VERSION 2.0.7.0
    HOMEPAGE_URL "https://github.com/hadoooooouken/qimgv-plus"
    LANGUAGES C CXX RC)

message(STATUS "Build configuration: " ${CMAKE_BUILD_TYPE})

# AUTOMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# OPTIMIZATIONS
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
    # Clean Release flags to perfectly map to Visual Studio UI properties
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /Oi /Ot /DNDEBUG")
    # Enable Whole Program Optimization (/GL) and Link Time Code Generation (/LTCG) natively in MSBuild/VS
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    message(STATUS "AVX2, full optimizations (/O2 /Ob2 /Oi /Ot), CRT warnings suppression and LTCG/GL enabled for Release")
endif()

# USER OPTIONS
option(EXIV2 "For reading/writing exif tags" ON)
option(OPENCV_SUPPORT "Enable HQ scaling via OpenCV" ON)
option(USE_QT5 "Build with Qt5 if available" ON)

# FIND PACKAGES
if(NOT USE_QT5)
    find_package(Qt6 COMPONENTS Widgets QUIET)
endif()

if(Qt6_FOUND AND NOT USE_QT5)
    set(QT_VERSION_MAJOR 6)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg SvgWidgets PrintSupport OpenGLWidgets Network Sql)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets Svg PrintSupport Network Sql)

    set(QT_VERSION_MAJOR 5)
endif()


if(EXIV2)
    # Hint to find the local exiv2 build in formats/exiv2
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/formats/exiv2")
    find_package(exiv2 REQUIRED)
endif()

if(OPENCV_SUPPORT)
    # Hint to find the local OpenCV build in formats/opencv-install
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/formats/opencv-install")
    set(OpenCV_STATIC ON)
    find_package(OpenCV REQUIRED core imgproc)
endif()

##############################################################

# --- UPSCAYL NCNN INTEGRATION ---
option(UPSCAYL_SUPPORT "Enable Upscayl NCNN integration" ON)

if(UPSCAYL_SUPPORT)
    message(STATUS "Configuring Upscayl NCNN library support using Vulkan SDK at E:/VULKAN")
    set(ENV{VULKAN_SDK} "E:/VULKAN")
    list(APPEND CMAKE_PREFIX_PATH "E:/VULKAN")
    
    # Configure NCNN options for Vulkan support
    set(NCNN_VULKAN ON CACHE BOOL "Build NCNN with Vulkan support" FORCE)
    set(NCNN_VULKAN_ONLINE_SPIRV ON CACHE BOOL "Build NCNN with Vulkan online SPIRV compilation support" FORCE)
    
    add_subdirectory(upscayl-ncnn/src)
endif()

add_subdirectory(qimgv)
add_subdirectory(shellex)


