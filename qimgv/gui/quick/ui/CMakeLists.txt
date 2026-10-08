## QML MODULE qimgv.ui
# Application shell of the Qt Quick UI. QML files here are presentation only;
# decisions stay in C++ components (see docs/QML_MIGRATION_PLAN.md).

qt_add_library(qimgv_ui STATIC)

qt_add_qml_module(qimgv_ui
    URI qimgv.ui
    QML_FILES
        Main.qml
)

target_link_libraries(qimgv_ui PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
)
