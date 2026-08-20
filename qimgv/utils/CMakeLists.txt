## SOURCES FOR QIMGV UTILITY CLASSES

set(QIMGV_ZSTD_ROOT "${CMAKE_SOURCE_DIR}/formats/zstd/install" CACHE PATH
    "Path to the local zstd installation used for Blender previews")
set(QIMGV_ZSTD_INCLUDE_DIR "${QIMGV_ZSTD_ROOT}/include")
set(QIMGV_ZSTD_LIBRARY "${QIMGV_ZSTD_ROOT}/lib/zstd.lib")
set(QIMGV_ZSTD_RUNTIME "${QIMGV_ZSTD_ROOT}/bin/zstd.dll")

if(NOT EXISTS "${QIMGV_ZSTD_INCLUDE_DIR}/zstd.h"
   OR NOT EXISTS "${QIMGV_ZSTD_LIBRARY}"
   OR NOT EXISTS "${QIMGV_ZSTD_RUNTIME}")
    message(FATAL_ERROR "zstd shared build not found under ${QIMGV_ZSTD_ROOT}")
endif()

target_include_directories(qimgv-plus PRIVATE "${QIMGV_ZSTD_INCLUDE_DIR}")
target_link_libraries(qimgv-plus PRIVATE "${QIMGV_ZSTD_LIBRARY}")

target_sources(qimgv-plus PRIVATE
    actions.cpp
    blendreader.cpp
    decodecontext.h
    djvureader.cpp
    fontpreview.cpp
    cmdoptionsrunner.cpp
    formatregistry.cpp
    formatgroups.cpp
    imagefactory.cpp
    imagelib.cpp
    inputmap.cpp
    randomizer.cpp
    script.cpp
    stuff.cpp
    fileoperations.cpp
    colormanager.cpp
    pngwriter.cpp
    iconfontmanager.cpp
    displayutils.cpp
)

target_sources(qimgv-plus PRIVATE
    ../gui/viewers/panoramagraphicsitem.cpp
    ../gui/viewers/panoramagraphicsitem.h
)
