## SOURCES FOR QIMGV UTILITY CLASSES

target_sources(qimgv-plus PRIVATE
    actions.cpp
    cmdoptionsrunner.cpp
    imagefactory.cpp
    imagelib.cpp
    inputmap.cpp
    randomizer.cpp
    script.cpp
    stuff.cpp
    fileoperations.cpp
)

target_sources(qimgv-plus PRIVATE
    ../gui/viewers/panoramagraphicsitem.cpp
    ../gui/viewers/panoramagraphicsitem.h
)
