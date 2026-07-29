## SOURCES FOR QIMGV COMPONENTS

target_sources(qimgv-plus PRIVATE
    directorymodel.cpp
    directorypresenter.cpp
    directoryexpandworker.cpp

    scriptmanager/scriptmanager.cpp
    actionmanager/actionmanager.cpp

    cache/cache.cpp
    cache/cacheitem.cpp
    cache/thumbnailcache.cpp
    cache/thumbnailcachemaintenance.cpp
    cache/thumbnailsourcestamp.cpp
    cache/thumbnailcachewriter.cpp

    loader/loader.cpp
    loader/loaderrunnable.cpp

    scaler/scaler.cpp
    scaler/scalerrunnable.cpp

    thumbnailer/thumbnailer.cpp
    thumbnailer/thumbnailerrunnable.cpp

    directorymanager/directorymanager.cpp

    directorymanager/watchers/directorywatcher.cpp
    directorymanager/watchers/watcherevent.cpp
    directorymanager/watchers/watcherworker.cpp

    batchconverter/batchconverter.cpp

    wallpaper/wallpapercontroller.cpp

    comfymetadata/comfyksamplerparser.cpp
    comfymetadata/pngtextchunkreader.cpp
)

# UPSCAYL SUPPORT
target_sources(qimgv-plus PRIVATE
    upscaler/upscaler.cpp
    upscaler/upscalerrunnable.cpp
    upscaler/upscaylresizerunnable.cpp
    upscaler/upscaylresizerunnable.h
)

if(APPLE)

    #target_sources(qimgv-plus PRIVATE
    #    directorymanager/watchers/linux/linuxfsevent.cpp
    #    directorymanager/watchers/linux/linuxwatcher.cpp
    #    directorymanager/watchers/linux/linuxworker.cpp)
elseif(UNIX)
    target_sources(qimgv-plus PRIVATE
        directorymanager/watchers/linux/linuxfsevent.cpp
        directorymanager/watchers/linux/linuxwatcher.cpp
        directorymanager/watchers/linux/linuxworker.cpp)
elseif(WIN32)
    target_sources(qimgv-plus PRIVATE
        directorymanager/watchers/windows/windowswatcher.cpp
        directorymanager/watchers/windows/windowsworker.cpp)
endif()
