## QML MODULE qimgv.render
# GPU image rendering for the Qt Quick UI. Its QQuickRhiItem-based types and
# RHI shaders are added by the Phase 1 stages of docs/QML_MIGRATION_PLAN.md.

qt_add_library(qimgv_render STATIC)

qt_add_qml_module(qimgv_render
    URI qimgv.render
    DEPENDENCIES QtQuick
)

target_link_libraries(qimgv_render PRIVATE
    Qt6::Quick
)
