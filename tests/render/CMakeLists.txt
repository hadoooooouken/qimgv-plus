## qimgv_render_tests (Qt Test)
# Pixel tests of ImageRenderItem / ImageRenderer. The item is rendered
# headless through QQuickRenderControl into a QRhi texture on Direct3D 11 and
# read back, so the tests run under the offscreen QPA platform.

qt_add_executable(qimgv_render_tests
    offscreenquick.cpp
    offscreenquick.h
    referenceimages.cpp
    referenceimages.h
    tst_imagerenderer.cpp
)

target_link_libraries(qimgv_render_tests PRIVATE
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Quick
    Qt6::Test
    qimgv_render
)

qimgv_configure_test(qimgv_render_tests)
