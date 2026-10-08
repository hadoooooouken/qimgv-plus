## qimgv_qml_tests (Qt Quick Test)
# Loads the qimgv.* QML modules through the same static plugins as
# qimgv-plus. Test files (tst_*.qml) are read from this source directory.

qt_add_executable(qimgv_qml_tests
    main.cpp
)

target_compile_definitions(qimgv_qml_tests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(qimgv_qml_tests PRIVATE
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickTest
    qimgv_uiplugin
)

qimgv_configure_test(qimgv_qml_tests)
