#pragma once

#include <QObject>

class ActionManager;
class ContextMenuModel;
class ImageViewportController;
class ScriptManager;

// Connects the Qt Quick context menu (ContextMenuModel) to the application:
// the menu action of ActionManager opens or closes it, after the scripts
// (those with a command, as the widget menu lists them), the displayed-image
// state and the CAS filter state were brought up to date. The script
// settings ("Configure menu") are part of the settings dialog, which the
// Qt Quick UI does not have yet (S3.3); the request is logged.
//
// All referenced objects must outlive this object. GUI thread only.
class QuickContextMenuActions final : public QObject {
    Q_OBJECT
public:
    QuickContextMenuActions(ActionManager &actions, ScriptManager &scripts,
                            ContextMenuModel &menu, ImageViewportController &viewport,
                            QObject *parent = nullptr);

private:
    void toggleMenu();

    ScriptManager &scripts;
    ContextMenuModel &menu;
    ImageViewportController &viewport;
};
