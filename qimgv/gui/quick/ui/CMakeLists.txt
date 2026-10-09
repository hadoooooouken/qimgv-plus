## QML MODULE qimgv.ui
# Application shell of the Qt Quick UI. QML files here are presentation only;
# decisions stay in C++ components (see docs/QML_MIGRATION_PLAN.md).

qt_add_library(qimgv_ui STATIC)

qt_add_qml_module(qimgv_ui
    URI qimgv.ui
    IMPORTS qimgv.bridges qimgv.render qimgv.style
    SOURCES
        framepresentationtracker.cpp
        framepresentationtracker.h
        imageviewportcontroller.cpp
        imageviewportcontroller.h
        mainwindowshell.cpp
        mainwindowshell.h
    QML_FILES
        ImageViewport.qml
        Main.qml
)

target_compile_features(qimgv_ui PUBLIC cxx_std_23)
if(MSVC)
    # Same C++26 working-draft switch as qimgv-plus.
    target_compile_options(qimgv_ui PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>")
endif()

# qmlcachegen compiles the bindings on the bridge singletons and the render
# item to C++ that calls them directly, so the module links their libraries.
# ImageViewportController (public: the Quick UI host and the tests use it)
# builds on the view components and drives the render item.
target_link_libraries(qimgv_ui
    PUBLIC
        Qt6::Quick
        qimgv_bridges
        qimgv_render
        qimgv_style
        qimgv_viewcomponents
    PRIVATE
        Qt6::QuickControls2
)
