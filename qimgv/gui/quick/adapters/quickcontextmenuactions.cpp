#include "quickcontextmenuactions.h"

#include <QDebug>
#include <QStringList>

#include "components/actionmanager/actionmanager.h"
#include "components/scriptmanager/scriptmanager.h"
#include "gui/quick/ui/imageviewportcontroller.h"
#include "gui/quick/ui/menus/contextmenumodel.h"

QuickContextMenuActions::QuickContextMenuActions(ActionManager &actions, ScriptManager &scripts,
                                                 ContextMenuModel &menu,
                                                 ImageViewportController &viewport,
                                                 QObject *parent)
    : QObject(parent), scripts(scripts), menu(menu), viewport(viewport) {
    connect(&actions, &ActionManager::contextMenu, this, &QuickContextMenuActions::toggleMenu);
    connect(&viewport, &ImageViewportController::imageChanged, this,
            [this]() { this->menu.setImageDisplayed(this->viewport.hasImage()); });
    connect(&menu, &ContextMenuModel::scriptSettingsRequested, this, []() {
        qInfo() << "Qt Quick UI: the script settings are not available yet";
    });
    menu.setImageDisplayed(viewport.hasImage());
}

void QuickContextMenuActions::toggleMenu() {
    QStringList names;
    const QMap<QString, Script> &all = scripts.allScripts();
    for (auto it = all.cbegin(); it != all.cend(); ++it) {
        if (!it.value().command.isEmpty())
            names << it.key();
    }
    menu.setScripts(names);
    menu.setImageDisplayed(viewport.hasImage());
    menu.setCasFilterActive(viewport.scalingFilter() == QI_FILTER_CAS);
    menu.toggle();
}
