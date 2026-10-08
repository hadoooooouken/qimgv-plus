## qimgv_tests (Qt Test)

qt_add_executable(qimgv_tests
    tst_qimgv.cpp
)

target_link_libraries(qimgv_tests PRIVATE
    Qt6::Core
    Qt6::Test
)

qimgv_configure_test(qimgv_tests)
