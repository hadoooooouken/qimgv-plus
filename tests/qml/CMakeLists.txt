## qimgv_qml_tests (Qt Quick Test)
# Loads the qimgv.* QML modules through the same static plugins as
# qimgv-plus. Test files (tst_*.qml) are read from this source directory.
# The executable is itself the qimgv.tests QML module, which provides the
# Fixture singleton (bridgetestfixture.h) the bridge tests drive.

qt_add_executable(qimgv_qml_tests
    main.cpp
)

qt_add_qml_module(qimgv_qml_tests
    URI qimgv.tests
    DEPENDENCIES qimgv.bridges qimgv.ui
    SOURCES
        bridgetestfixture.cpp
        bridgetestfixture.h
)

target_compile_definitions(qimgv_qml_tests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(qimgv_qml_tests PRIVATE
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickTest
    qimgv_bridges
    qimgv_ui
    qimgv_bridgesplugin
    qimgv_uiplugin
    qimgv_renderplugin
)

qimgv_configure_test(qimgv_qml_tests)
