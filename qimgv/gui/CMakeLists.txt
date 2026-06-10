## SOURCES FOR QIMGV GUI

target_sources(qimgv-plus PRIVATE
    centralwidget.cpp
    contextmenu.cpp
    flowlayout.cpp
    idirectoryview.cpp
    mainwindow.cpp

    customwidgets/actionbutton.cpp
    customwidgets/clickablelabel.cpp
    customwidgets/contextmenuitem.cpp
    customwidgets/floatingwidget.cpp
    customwidgets/floatingwidgetcontainer.cpp
    customwidgets/iconbutton.cpp
    customwidgets/iconwidget.cpp
    customwidgets/keysequenceedit.cpp
    customwidgets/menuitem.cpp
    customwidgets/overlaywidget.cpp
    customwidgets/pathselectormenuitem.cpp
    customwidgets/sidepanelwidget.cpp
    customwidgets/slidepanel.cpp
    customwidgets/spinboxinputfix.cpp
    customwidgets/thumbnailview.cpp
    customwidgets/thumbnailwidget.cpp
    customwidgets/pushbuttonfocusind.cpp
    customwidgets/styledcombobox.cpp
    customwidgets/colorselectorbutton.cpp
    customwidgets/entryinfoitem.cpp
    customwidgets/ssidebar.cpp

    dialogs/filereplacedialog.cpp
    dialogs/batchconverterdialog.cpp
    dialogs/resizedialog.cpp
    dialogs/scripteditordialog.cpp
    dialogs/settingsdialog.cpp
    dialogs/shortcutcreatordialog.cpp
    dialogs/printdialog.cpp

    overlays/clickzoneoverlay.cpp
    overlays/controlsoverlay.cpp
    overlays/copyoverlay.cpp
    overlays/cropoverlay.cpp
    overlays/floatingmessage.cpp
    overlays/floatingmessageproxy.cpp
    overlays/fullscreeninfooverlay.cpp
    overlays/fullscreeninfooverlayproxy.cpp
    overlays/imageinfooverlay.cpp
    overlays/imageinfooverlayproxy.cpp
    overlays/cassettingsoverlay.cpp
    overlays/coloradjustmentsoverlay.cpp
    overlays/coloradjustmentsoverlayproxy.cpp
    overlays/mapoverlay.cpp
    overlays/renameoverlay.cpp
    overlays/saveconfirmoverlay.cpp
    overlays/zoomindicatoroverlay.cpp
    overlays/zoomindicatoroverlayproxy.cpp

    panels/croppanel/croppanel.cpp
    panels/mainpanel/mainpanel.cpp
    panels/mainpanel/thumbnailstrip.cpp
    panels/mainpanel/thumbnailstripproxy.cpp
    panels/sidepanel/sidepanel.cpp

    panels/infobar/infobar.cpp
    panels/infobar/infobarproxy.cpp

    viewers/documentwidget.cpp
    viewers/filterpixmapitem.cpp
    viewers/imageviewerv2.cpp
    viewers/viewerwidget.cpp

    folderview/foldergridview.cpp
    folderview/folderview.cpp
    folderview/folderviewproxy.cpp
    folderview/filesystemmodelcustom.cpp
    folderview/treeviewcustom.cpp
    folderview/bookmarkswidget.cpp
    folderview/bookmarksitem.cpp
)

# UI Files

target_sources(qimgv-plus PRIVATE

    dialogs/batchconverterdialog.ui
    dialogs/settingsdialog.ui
    overlays/renameoverlay.ui

    panels/croppanel/croppanel.ui
    panels/sidepanel/sidepanel.ui

    panels/infobar/infobar.ui

    folderview/folderview.ui
)
