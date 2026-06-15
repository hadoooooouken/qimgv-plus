## SOURCES FOR QIMGV COMPONENTS

target_sources(qimgv-plus PRIVATE
    directorymodel.cpp
    directorypresenter.cpp

    scriptmanager/scriptmanager.cpp
    actionmanager/actionmanager.cpp

    cache/cache.cpp
    cache/cacheitem.cpp
    cache/thumbnailcache.cpp

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

)

if(UPSCAYL_SUPPORT)
    target_sources(qimgv-plus PRIVATE
        upscaler/upscaler.cpp
        upscaler/upscalerrunnable.cpp
    )
endif()

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
