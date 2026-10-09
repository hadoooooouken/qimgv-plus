#include "quickcropactions.h"

#include "gui/ports/uievents.h"
#include "gui/quick/ui/crop/cropcontroller.h"
#include "gui/quick/ui/imageviewportcontroller.h"
#include "gui/quick/ui/menus/contextmenumodel.h"
#include "gui/quick/ui/thumbnails/thumbnailpanelcontroller.h"
#include "settings.h"

QuickCropActions::QuickCropActions(const QuickCropContext &context, QObject *parent)
    : QObject(parent),
      crop(context.crop),
      viewport(context.viewport),
      thumbnailPanel(context.thumbnailPanel),
      contextMenu(context.contextMenu) {
    connect(&crop, &CropController::activeChanged, this, &QuickCropActions::onActiveChanged);
    connect(&viewport, &ImageViewportController::imageChanged, this,
            &QuickCropActions::syncImage);
    connect(&viewport, &ImageViewportController::imageGeometryChanged, this,
            &QuickCropActions::syncGeometry);

    UiEvents *events = &context.events;
    connect(&crop, &CropController::cropRequested, events, &UiEvents::cropRequested);
    connect(&crop, &CropController::cropAndSaveRequested, events,
            &UiEvents::cropAndSaveRequested);
    Settings *settings = &context.settings;
    connect(&crop, &CropController::defaultActionChosen, settings,
            [settings](SettingsEnums::CropAction action) {
                settings->setDefaultCropAction(static_cast<DefaultCropAction>(action));
            });

    syncImage();
    syncGeometry();
}

void QuickCropActions::onActiveChanged() {
    const bool active = crop.isActive();
    if (active) {
        viewport.setExpandSmallImagesInFitMode(true);
        viewport.fitWindow();
    } else {
        viewport.setExpandSmallImagesInFitMode(false);
    }
    viewport.setInteractionEnabled(!active);
    contextMenu.setInteractionEnabled(!active);
    thumbnailPanel.setInteractionEnabled(!active);
    syncGeometry();
}

void QuickCropActions::syncImage() {
    crop.setImageSize(viewport.imageSize());
    syncGeometry();
}

void QuickCropActions::syncGeometry() {
    crop.setDevicePixelRatio(viewport.devicePixelRatio());
    crop.setImageArea(viewport.imageArea());
}
