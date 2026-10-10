## SOURCES FOR QIMGV UTILITY CLASSES

# ZSTD_INSTALL_DIR comes from qimgv/CMakeLists.txt.
set(QIMGV_ZSTD_INCLUDE_DIR "${ZSTD_INSTALL_DIR}/include")
set(QIMGV_ZSTD_LIBRARY "${ZSTD_INSTALL_DIR}/lib/zstd.lib")
set(QIMGV_ZSTD_RUNTIME "${ZSTD_INSTALL_DIR}/bin/zstd.dll")

if(NOT EXISTS "${QIMGV_ZSTD_INCLUDE_DIR}/zstd.h"
   OR NOT EXISTS "${QIMGV_ZSTD_LIBRARY}"
   OR NOT EXISTS "${QIMGV_ZSTD_RUNTIME}")
    message(FATAL_ERROR "zstd shared build not found under ${ZSTD_INSTALL_DIR}")
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
    hdrtonemapper.cpp
    hdrtonemapper.h
)
