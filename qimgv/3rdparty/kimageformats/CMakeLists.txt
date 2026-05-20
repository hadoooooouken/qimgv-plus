# kimageformats for qimgv

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

function(kimageformats_add_plugin plugin)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(KIF_ADD_PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(QT_VERSION_MAJOR EQUAL 6)
        qt_add_plugin(${plugin} PLUGIN_TYPE imageformats)
    else()
        qt5_add_plugin(${plugin})
    endif()

    target_sources(${plugin} PRIVATE ${KIF_ADD_PLUGIN_SOURCES})
    target_link_libraries(${plugin} PRIVATE Qt${QT_VERSION_MAJOR}::Gui)

    # Output to imageformats/ folder next to the executable
    set_target_properties(${plugin} PROPERTIES 
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/imageformats"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/imageformats"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/imageformats"
    )
endfunction()

# Standalone plugins
kimageformats_add_plugin(kimg_ani SOURCES ani.cpp)
kimageformats_add_plugin(kimg_dds SOURCES dds.cpp scanlineconverter.cpp)
kimageformats_add_plugin(kimg_ff SOURCES ff.cpp)
kimageformats_add_plugin(kimg_hdr SOURCES hdr.cpp)
kimageformats_add_plugin(kimg_iff SOURCES iff.cpp chunks.cpp microexif.cpp)
kimageformats_add_plugin(kimg_pcx SOURCES pcx.cpp scanlineconverter.cpp)
kimageformats_add_plugin(kimg_pic SOURCES pic.cpp scanlineconverter.cpp)
kimageformats_add_plugin(kimg_pfm SOURCES pfm.cpp)
kimageformats_add_plugin(kimg_psd SOURCES psd.cpp microexif.cpp scanlineconverter.cpp)
kimageformats_add_plugin(kimg_pxr SOURCES pxr.cpp)
kimageformats_add_plugin(kimg_qoi SOURCES qoi.cpp scanlineconverter.cpp)
kimageformats_add_plugin(kimg_ras SOURCES ras.cpp)
kimageformats_add_plugin(kimg_rgb SOURCES rgb.cpp scanlineconverter.cpp)
kimageformats_add_plugin(kimg_sct SOURCES sct.cpp)
kimageformats_add_plugin(kimg_tim SOURCES tim.cpp)
kimageformats_add_plugin(kimg_tga SOURCES tga.cpp microexif.cpp scanlineconverter.cpp)
kimageformats_add_plugin(kimg_xcf SOURCES xcf.cpp)
kimageformats_add_plugin(kimg_kra SOURCES kra.cpp miniz.c miniz_zip.c miniz_tdef.c miniz_tinfl.c)
kimageformats_add_plugin(kimg_ora SOURCES ora.cpp miniz.c miniz_zip.c miniz_tdef.c miniz_tinfl.c)
# OpenEXR (Enabled with local build)
set(OPENEXR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/openexr/install")
set(IMATH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/Imath/install")
kimageformats_add_plugin(kimg_exr SOURCES exr.cpp scanlineconverter.cpp)
target_include_directories(kimg_exr SYSTEM PRIVATE 
    "${OPENEXR_ROOT}/include"
    "${OPENEXR_ROOT}/include/OpenEXR"
    "${IMATH_ROOT}/include"
    "${IMATH_ROOT}/include/Imath"
)
target_link_libraries(kimg_exr PRIVATE 
    "${OPENEXR_ROOT}/lib/OpenEXR-4_0.lib"
    "${OPENEXR_ROOT}/lib/Iex-4_0.lib"
    "${OPENEXR_ROOT}/lib/IlmThread-4_0.lib"
    "${IMATH_ROOT}/lib/Imath-3_2.lib"
)

# LibRaw (Enabled with local build)
set(LIBRAW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/LibRaw")
kimageformats_add_plugin(kimg_raw SOURCES raw.cpp scanlineconverter.cpp)
target_include_directories(kimg_raw PRIVATE "${LIBRAW_ROOT}")
target_link_libraries(kimg_raw PRIVATE "${LIBRAW_ROOT}/buildfiles/release-x86_64/libraw.lib")

# JXR (Enabled with local build using jxrlib)
set(JXR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/jxrlib/install")
if(EXISTS "${JXR_ROOT}/lib/jxrglue.lib")
    kimageformats_add_plugin(kimg_jxr SOURCES jxr.cpp microexif.cpp)
    target_include_directories(kimg_jxr SYSTEM PRIVATE "${JXR_ROOT}/include/jxrlib")
    target_link_libraries(kimg_jxr PRIVATE 
        "${JXR_ROOT}/lib/jxrglue.lib"
        "${JXR_ROOT}/lib/jpegxr.lib"
    )
endif()

if(MSVC)
    target_compile_options(kimg_exr PRIVATE /wd4996)
    if(TARGET kimg_jxr)
        target_compile_options(kimg_jxr PRIVATE /wd4828)
    endif()
endif()

message(STATUS "Plugins configured: ani dds ff hdr iff pcx pic pfm psd pxr qoi ras rgb sct tim tga xcf exr raw kra ora jxr")

# Advanced formats (AVIF, HEIF, JXL)
# libavif
set(AVIF_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/libavif/include")
set(AVIF_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/libavif/build/Release/avif.lib")
if(EXISTS "${AVIF_LIBRARY}")
    kimageformats_add_plugin(kimg_avif SOURCES avif.cpp microexif.cpp)
    target_include_directories(kimg_avif PRIVATE ${AVIF_INCLUDE_DIR})
    target_link_libraries(kimg_avif PRIVATE ${AVIF_LIBRARY})
endif()

# libheif
set(HEIF_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/libheif/libheif/api")
set(HEIF_BUILD_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/libheif/build")
set(HEIF_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/libheif/build/libheif/Release/heif.lib")
if(EXISTS "${HEIF_LIBRARY}")
    kimageformats_add_plugin(kimg_heif SOURCES heif.cpp microexif.cpp)
    target_include_directories(kimg_heif PRIVATE ${HEIF_INCLUDE_DIR} ${HEIF_BUILD_INCLUDE_DIR})
    target_link_libraries(kimg_heif PRIVATE ${HEIF_LIBRARY})
endif()

# libjxl
set(JXL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/libjxl/lib/include")
set(JXL_BUILD_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/libjxl/build/lib/include")
set(JXL_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/libjxl/build/lib/Release/jxl.lib")
set(JXL_THREADS_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/libjxl/build/lib/Release/jxl_threads.lib")
set(JXL_CMS_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../../../formats/libjxl/build/lib/Release/jxl_cms.lib")
if(EXISTS "${JXL_LIBRARY}")
    kimageformats_add_plugin(kimg_jxl SOURCES jxl.cpp microexif.cpp)
    target_include_directories(kimg_jxl PRIVATE ${JXL_INCLUDE_DIR} ${JXL_BUILD_INCLUDE_DIR})
    target_link_libraries(kimg_jxl PRIVATE ${JXL_LIBRARY} ${JXL_THREADS_LIBRARY} ${JXL_CMS_LIBRARY})
endif()
