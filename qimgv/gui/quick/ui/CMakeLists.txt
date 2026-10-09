## QML MODULE qimgv.ui
# Application shell of the Qt Quick UI. QML files here are presentation only;
# decisions stay in C++ components (see docs/QML_MIGRATION_PLAN.md).

qt_add_library(qimgv_ui STATIC)

qt_add_qml_module(qimgv_ui
    URI qimgv.ui
    IMPORTS qimgv.bridges qimgv.render qimgv.style
    SOURCES
        framepresentationtracker.cpp
        framepresentationtracker.h
        imageviewportcontroller.cpp
        imageviewportcontroller.h
        mainwindowshell.cpp
        mainwindowshell.h
        overlays/adjustmentslidermodel.cpp
        overlays/adjustmentslidermodel.h
        overlays/cassettingseditor.cpp
        overlays/cassettingseditor.h
        overlays/coloradjustmentseditor.cpp
        overlays/coloradjustmentseditor.h
        overlays/copytargetsmodel.cpp
        overlays/copytargetsmodel.h
        overlays/fullscreenchromecontroller.cpp
        overlays/fullscreenchromecontroller.h
        overlays/imageinfomodel.cpp
        overlays/imageinfomodel.h
        overlays/notificationoverlaymodel.cpp
        overlays/notificationoverlaymodel.h
        overlays/overlaycoordinator.cpp
        overlays/overlaycoordinator.h
        overlays/overlaystate.cpp
        overlays/overlaystate.h
        overlays/renamepromptcontroller.cpp
        overlays/renamepromptcontroller.h
    QML_FILES
        AdjustmentSliders.qml
        CasSettingsOverlay.qml
        ColorAdjustmentsOverlay.qml
        ControlsOverlay.qml
        CopyOverlay.qml
        FloatingMessage.qml
        FullscreenInfoOverlay.qml
        ImageInfoOverlay.qml
        ImageViewport.qml
        Main.qml
        OverlayHeaderButton.qml
        OverlayLayer.qml
        OverlayPanel.qml
        RenameOverlay.qml
        SaveConfirmOverlay.qml
)

# The generated QML type registrations include the headers by file name.
target_include_directories(qimgv_ui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/overlays")
target_compile_features(qimgv_ui PUBLIC cxx_std_23)
if(MSVC)
    # Same C++26 working-draft switch as qimgv-plus.
    target_compile_options(qimgv_ui PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>")
endif()

# qmlcachegen compiles the bindings on the bridge singletons and the render
# item to C++ that calls them directly, so the module links their libraries.
# ImageViewportController (public: the Quick UI host and the tests use it)
# builds on the view components and drives the render item.
target_link_libraries(qimgv_ui
    PUBLIC
        Qt6::Quick
        qimgv_bridges
        qimgv_render
        qimgv_style
        qimgv_viewcomponents
    PRIVATE
        Qt6::QuickControls2
        # FolderDialog of the copy / move overlay.
        Qt6::QuickDialogs2
)
