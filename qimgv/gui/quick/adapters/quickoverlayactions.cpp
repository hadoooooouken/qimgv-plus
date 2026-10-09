#include "quickoverlayactions.h"

#include <QDir>
#include <QStringList>

#include "components/actionmanager/actionmanager.h"
#include "components/copytargets/copytargetlist.h"
#include "gui/quick/ui/imageviewportcontroller.h"
#include "gui/quick/ui/overlays/overlaycoordinator.h"
#include "settings.h"

namespace {
using namespace Qt::StringLiterals;

// Actions whose shortcuts still work while the rename field has the focus
// (RenameOverlay's key filter).
const QStringList &renamePassThroughActions() {
    static const QStringList actions{u"exit"_s, u"renameFile"_s};
    return actions;
}
} // namespace

QuickOverlayActions::QuickOverlayActions(ActionManager &actions, Settings &settings,
                                         OverlayCoordinator &overlays,
                                         ImageViewportController &viewport, QObject *parent)
    : QObject(parent),
      actions(actions),
      settings(settings),
      overlays(overlays),
      viewport(viewport) {
    connectActions();
    connectModels();
    refreshPassThroughShortcuts();
    overlays.setDocumentDisplayed(viewport.hasImage());
}

void QuickOverlayActions::connectActions() {
    OverlayCoordinator *coordinator = &overlays;
    connect(&actions, &ActionManager::copyFile, coordinator, &OverlayCoordinator::toggleCopy);
    connect(&actions, &ActionManager::moveFile, coordinator, &OverlayCoordinator::toggleMove);
    connect(&actions, &ActionManager::toggleImageInfo, coordinator,
            &OverlayCoordinator::toggleImageInfo);
    connect(&actions, &ActionManager::colorAdjustments, coordinator,
            &OverlayCoordinator::toggleColorAdjustments);
    connect(&actions, &ActionManager::casSettings, coordinator,
            &OverlayCoordinator::toggleCasSettings);
}

void QuickOverlayActions::connectModels() {
    connect(&overlays, &OverlayCoordinator::copyTargetsNeeded, this,
            &QuickOverlayActions::loadCopyTargets);
    connect(overlays.copyTargets(), &CopyTargetsModel::targetsEdited, this,
            [this](const QStringList &targets) { settings.setSavedPaths(targets); });

    connect(&overlays, &OverlayCoordinator::casParametersEdited, this,
            [this](const CasParameters &parameters) {
                settings.setCasSharpening(parameters.sharpening);
                settings.setCasContrast(parameters.contrast);
                viewport.setCasParameters(parameters.sharpening, parameters.contrast);
            });
    connect(overlays.colorAdjustmentsEditor(), &ColorAdjustmentsEditor::previewChanged,
            &viewport, &ImageViewportController::setColorAdjustments);
    connect(overlays.fullscreenChrome(), &FullscreenChromeController::infoBarSettingToggled,
            this, [this](bool enabled) { settings.setInfoBarFullscreen(enabled); });

    connect(&viewport, &ImageViewportController::imageChanged, this,
            [this]() { overlays.setDocumentDisplayed(viewport.hasImage()); });
    // Shortcut edits are announced with the other settings changes.
    connect(&settings, &Settings::settingsChanged, this,
            &QuickOverlayActions::refreshPassThroughShortcuts);
}

// Lists the home folder on first use only, never at startup.
void QuickOverlayActions::loadCopyTargets() {
    overlays.copyTargets()->setTargets(copyTargetsFrom(settings.savedPaths(), QDir::homePath()));
}

void QuickOverlayActions::refreshPassThroughShortcuts() {
    QStringList shortcuts;
    for (const QString &action : renamePassThroughActions())
        shortcuts << actions.shortcutsForAction(action);
    overlays.renamePrompt()->setPassThroughShortcuts(shortcuts);
}
