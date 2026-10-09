## qimgv_tests (Qt Test)

set(QIMGV_APP_SOURCE_DIR "${PROJECT_SOURCE_DIR}/qimgv")

qt_add_executable(qimgv_tests
    main.cpp
    tst_animationplayer.cpp
    tst_colortransform.cpp
    tst_qimgv.cpp
    testanimations.h
    tst_imagefilter.cpp
    tst_imageviewportcontroller.cpp
    tst_tilegrid.cpp
    tst_viewportinteraction.cpp
    tst_viewtransform.cpp
    tst_decodedpixels.cpp
    tst_quickshell.cpp
    tst_singleinstance.cpp
    tst_windowstate.cpp
    tst_upscaledecision.cpp
    tst_windowtitle.cpp
    tst_uimetrics.cpp
    tst_overlays.cpp
    tst_thumbnailstrip.cpp
    tst_contextmenu.cpp
    tst_crop.cpp
    tst_svgrasterizer.cpp
    "${QIMGV_APP_SOURCE_DIR}/components/viewmode/viewmodecontroller.cpp"
    "${QIMGV_APP_SOURCE_DIR}/components/shellinfo/windowtitle.cpp"
    "${QIMGV_APP_SOURCE_DIR}/components/singleinstance/singleinstancechannel.cpp"
    "${QIMGV_APP_SOURCE_DIR}/components/upscaler/upscaledecision.cpp"
    "${QIMGV_APP_SOURCE_DIR}/components/windowstate/windowstatecontroller.cpp"
    "${QIMGV_APP_SOURCE_DIR}/sourcecontainers/decodedpixels.cpp"
    "${QIMGV_APP_SOURCE_DIR}/sourcecontainers/thumbnail.cpp"
    "${QIMGV_APP_SOURCE_DIR}/gui/quick/adapters/directoryviewadapter.cpp"
    "${QIMGV_APP_SOURCE_DIR}/gui/quick/adapters/placeholderdirectoryview.cpp"
    "${QIMGV_APP_SOURCE_DIR}/gui/quick/adapters/themesnapshotbuilder.cpp"
    "${QIMGV_APP_SOURCE_DIR}/themestore.cpp"
    "${QIMGV_APP_SOURCE_DIR}/utils/actions.cpp"
)

# Components under test are compiled from the application sources and use
# the application's include root.
target_include_directories(qimgv_tests PRIVATE "${QIMGV_APP_SOURCE_DIR}")

target_link_libraries(qimgv_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    # SingleInstanceChannel (QLocalServer / QLocalSocket).
    Qt6::Network
    Qt6::Test
    Qt6::Quick
    # TileGrid, ImageFilterMode (moc data of RenderEnums), colour transform
    # plans and ColorLutBuilder.
    qimgv_render
    # ImageViewportController and FramePresentationTracker; the view
    # components (ViewTransform, ViewportInteraction, AnimationPlayer, ...)
    # come with it.
    qimgv_ui
)

qimgv_configure_test(qimgv_tests)
