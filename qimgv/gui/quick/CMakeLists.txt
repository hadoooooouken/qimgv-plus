## QT QUICK UI MODULES
# Each QML module of the Qt Quick UI (--ui=quick) is a static library linked
# into qimgv-plus through its static plugin; see quickuihost.cpp.

# Module output directories follow the module URIs (qimgv/ui, qimgv/render)
# below one shared root, so that qmllint, qmlcachegen and qmlls resolve the
# qimgv.* imports of each module from the build tree.
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# Qt generates helper targets for every module (static plugin, plugin init,
# resource and cache object libraries) that are linked into qimgv-plus and
# need the same compile profile as the explicitly created module targets.
qimgv_apply_msvc_directory_profile()

add_subdirectory(render)
add_subdirectory(ui)
