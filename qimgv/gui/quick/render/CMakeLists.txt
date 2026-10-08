## QML MODULE qimgv.render
# GPU image rendering for the Qt Quick UI (Phase 1 of
# docs/QML_MIGRATION_PLAN.md): ImageRenderItem (QQuickRhiItem) and its
# render-thread ImageRenderer, drawing through QRhi with the RHI shaders in
# res/shaders/rhi (tile drawing with sharpening and colour adjustments, and
# the exact-ratio box downsample).

qt_add_library(qimgv_render STATIC)

qt_add_qml_module(qimgv_render
    URI qimgv.render
    DEPENDENCIES QtQuick
    SOURCES
        imagefiltermode.cpp
        imagefiltermode.h
        imagerenderer.cpp
        imagerenderer.h
        imagerenderitem.cpp
        imagerenderitem.h
        renderframe.h
        rendererrorchannel.cpp
        rendererrorchannel.h
        textureuploadformat.cpp
        textureuploadformat.h
        tilegrid.cpp
        tilegrid.h
)

# GLSL 440 sources compiled to .qsb (SPIR-V, HLSL and GLSL variants) for
# every QRhi backend; ImageRenderer loads them from :/qimgv/render/shaders.
set(QIMGV_RHI_SHADER_DIR "${PROJECT_SOURCE_DIR}/qimgv/res/shaders/rhi")
qt_add_shaders(qimgv_render "qimgv_render_shaders"
    PREFIX "/qimgv/render/shaders"
    BASE "${QIMGV_RHI_SHADER_DIR}"
    FILES
        "${QIMGV_RHI_SHADER_DIR}/image.vert"
        "${QIMGV_RHI_SHADER_DIR}/image.frag"
        "${QIMGV_RHI_SHADER_DIR}/boxreduce.vert"
        "${QIMGV_RHI_SHADER_DIR}/boxreduce.frag"
)

# Headers include each other relative to the application root.
target_include_directories(qimgv_render PUBLIC "${PROJECT_SOURCE_DIR}/qimgv")

target_compile_features(qimgv_render PUBLIC cxx_std_23)
if(MSVC)
    # Same C++26 working-draft switch as qimgv-plus.
    target_compile_options(qimgv_render PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>")
endif()

# QRhi (<rhi/qrhi.h>) is published through GuiPrivate, with limited
# compatibility guarantees across Qt minor versions.
target_link_libraries(qimgv_render
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::GuiPrivate
        Qt6::Qml
        Qt6::Quick
)
