## qimgv_tests (Qt Test)

set(QIMGV_APP_SOURCE_DIR "${PROJECT_SOURCE_DIR}/qimgv")

qt_add_executable(qimgv_tests
    tst_qimgv.cpp
    "${QIMGV_APP_SOURCE_DIR}/components/viewmode/viewmodecontroller.cpp"
)

# Components under test are compiled from the application sources and use
# the application's include root.
target_include_directories(qimgv_tests PRIVATE "${QIMGV_APP_SOURCE_DIR}")

target_link_libraries(qimgv_tests PRIVATE
    Qt6::Core
    Qt6::Test
)

qimgv_configure_test(qimgv_tests)
