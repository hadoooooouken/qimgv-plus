## qimgv_qml_tests (Qt Quick Test)
# Loads the qimgv.* QML modules through the same static plugins as
# qimgv-plus. Test files (tst_*.qml) are read from this source directory.
# The theme snapshots are built from the application's own colour schemes
# (ThemeStore) and the icon font is loaded from the source tree, so the style
# tests see the real light and dark themes.
# The executable is itself the qimgv.tests QML module, which provides the
# Fixture singleton (bridgetestfixture.h) the bridge tests drive.

set(QIMGV_APP_SOURCE_DIR "${PROJECT_SOURCE_DIR}/qimgv")

qt_add_executable(qimgv_qml_tests
    main.cpp
    "${QIMGV_APP_SOURCE_DIR}/themestore.cpp"
    "${QIMGV_APP_SOURCE_DIR}/gui/quick/adapters/themesnapshotbuilder.cpp"
)

qt_add_qml_module(qimgv_qml_tests
    URI qimgv.tests
    DEPENDENCIES qimgv.bridges qimgv.style qimgv.ui
    SOURCES
        bridgetestfixture.cpp
        bridgetestfixture.h
)

# In-memory stores of the settings editor (shared with qimgv_tests).
target_include_directories(qimgv_qml_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../fakes")

target_compile_definitions(qimgv_qml_tests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    QIMGV_ICON_FONT_FILE="${QIMGV_APP_SOURCE_DIR}/res/fonts/FluentSystemIcons-Custom.ttf"
)

target_link_libraries(qimgv_qml_tests PRIVATE
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickTest
    # QTest key clicks into the dialog windows.
    Qt6::Test
    qimgv_bridges
    qimgv_style
    qimgv_ui
    qimgv_bridgesplugin
    qimgv_styleplugin
    qimgv_uiplugin
    qimgv_renderplugin
)

qimgv_configure_test(qimgv_qml_tests)
