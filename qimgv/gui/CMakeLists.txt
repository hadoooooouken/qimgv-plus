## SOURCES FOR QIMGV GUI

target_sources(qimgv-plus PRIVATE
    idirectoryview.h

    controllers/coldstartwindowcontroller.cpp

    ports/dialogport.h
    ports/notificationport.h
    ports/shellport.h
    ports/uievents.h
    ports/uiports.h
    ports/viewerport.h
    ports/viewmodeport.h
    ports/windowport.h

    # Settings dialog stores over Settings / ActionManager / ScriptManager.
    settingseditor/appsettingsstore.cpp
    settingseditor/appsettingsstore.h

    quick/quickuihost.cpp
    quick/adapters/actionmanagerdispatcher.cpp
    quick/adapters/appbatchconversionservice.cpp
    quick/adapters/bridgesnapshots.cpp
    quick/adapters/directoryviewadapter.cpp
    quick/adapters/filesystemmodelcustom.cpp
    quick/adapters/quickcontextmenuactions.cpp
    quick/adapters/quickcropactions.cpp
    quick/adapters/quickdialogport.cpp
    quick/adapters/quickfolderviewactions.cpp
    quick/adapters/quickmainwindowcontroller.cpp
    quick/adapters/quickoverlayactions.cpp
    quick/adapters/quickvieweractions.cpp
    quick/adapters/quickviewerport.cpp
    quick/adapters/themesnapshotbuilder.cpp
)

add_subdirectory(quick)
