## qimgv_tests (Qt Test)

set(QIMGV_APP_SOURCE_DIR "${PROJECT_SOURCE_DIR}/qimgv")

qt_add_executable(qimgv_tests
    main.cpp
    tst_qimgv.cpp
    tst_imagefilter.cpp
    tst_tilegrid.cpp
    tst_viewtransform.cpp
    "${QIMGV_APP_SOURCE_DIR}/components/viewmode/viewmodecontroller.cpp"
    "${QIMGV_APP_SOURCE_DIR}/components/viewtransform/viewtransform.cpp"
    "${QIMGV_APP_SOURCE_DIR}/components/viewtransform/viewtransformcontroller.cpp"
)

# Components under test are compiled from the application sources and use
# the application's include root.
target_include_directories(qimgv_tests PRIVATE "${QIMGV_APP_SOURCE_DIR}")

target_link_libraries(qimgv_tests PRIVATE
    Qt6::Core
    Qt6::Test
    # TileGrid, ImageFilterMode (moc data of RenderEnums).
    qimgv_render
)

qimgv_configure_test(qimgv_tests)
