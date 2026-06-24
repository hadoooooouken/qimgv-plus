# shellex/CMakeLists.txt
cmake_minimum_required(VERSION 3.25)

project(qimgvshellex LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

# Find Qt
find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_library(qimgvshellex SHARED
    qimgvshellex.h
    qimgvshellex.cpp
    qimgvshellex.def
)

# Standard C++ features
target_compile_features(qimgvshellex PRIVATE cxx_std_23)

if(MSVC)
    target_compile_options(qimgvshellex PRIVATE /std:c++latest)
endif()

# Disable Qt version tagging to allow delay-loading (prevents data symbol imports)
target_compile_definitions(qimgvshellex PRIVATE QT_NO_VERSION_TAGGING)

# Target properties
set_target_properties(qimgvshellex PROPERTIES
    CXX_EXTENSIONS OFF
    PREFIX "" # No "lib" prefix on Windows (output as qimgvshellex.dll directly)
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../release"
)

# Link Qt, LibRaw, and standard Win32 libraries
target_include_directories(qimgvshellex PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../formats/LibRaw"
)
target_link_libraries(qimgvshellex PRIVATE
    Qt6::Core
    Qt6::Gui
    shlwapi
    "${CMAKE_CURRENT_SOURCE_DIR}/../formats/LibRaw/buildfiles/release-x86_64/libraw_static.lib"
)
target_compile_definitions(qimgvshellex PRIVATE LIBRAW_NODLL)

# Add .def file and delay-loading to MSVC linker flags
if(MSVC)
    target_link_options(qimgvshellex PRIVATE 
        "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/qimgvshellex.def"
        "/DELAYLOAD:Qt6Core.dll"
        "/DELAYLOAD:Qt6Gui.dll"
    )
    target_link_libraries(qimgvshellex PRIVATE delayimp)
endif()
