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
    "${QIMGV_APP_SOURCE_DIR}/components/viewmode/viewmodecontroller.cpp"
)

# Components under test are compiled from the application sources and use
# the application's include root.
target_include_directories(qimgv_tests PRIVATE "${QIMGV_APP_SOURCE_DIR}")

target_link_libraries(qimgv_tests PRIVATE
    Qt6::Core
    Qt6::Gui
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
