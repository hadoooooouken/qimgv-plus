#include "viewertoggles.h"

#include <QCoreApplication>

// The messages reuse the widget UI's translations, which lupdate files under
// the main window's context ("MW"); lupdate needs the literal context in
// every QCoreApplication::translate() call.

ViewerToggles::ViewerToggles(IViewerToggleStore &store, QObject *parent)
    : QObject(parent),
      store(store) {
    modelSwitchTimer.setSingleShot(true);
    modelSwitchTimer.setInterval(kModelSwitchDelayMs);
    connect(&modelSwitchTimer, &QTimer::timeout, this, &ViewerToggles::publishModelSwitch);
}

void ViewerToggles::toggleUpscayl() {
    const bool enabled = !store.useUpscayl();
    store.setUseUpscayl(enabled);
    store.publishChanges();
    emit notificationRequested({enabled ? QCoreApplication::translate("MW", "Use Upscayl: ON")
                                        : QCoreApplication::translate("MW", "Use Upscayl: OFF"),
                                NotificationKind::AiUpscale, kToggleNotificationMs});
    if (!enabled)
        emit upscaledCropHideRequested();
}

void ViewerToggles::cycleUpscaylModel() {
    const QStringList models = store.availableUpscaylModels();
    if (models.isEmpty())
        return;
    const QString current = pendingModel.isEmpty() ? store.upscaylModel() : pendingModel;
    // An unknown current model (indexOf() == -1) starts over at the first.
    const qsizetype next = (models.indexOf(current) + 1) % models.size();
    pendingModel = models.at(next);
    store.setUpscaylModel(pendingModel);
    emit notificationRequested({QCoreApplication::translate("MW", "Model: %1").arg(pendingModel),
                                NotificationKind::AiUpscale, std::nullopt});
    modelSwitchTimer.start();
}

void ViewerToggles::toggleHdrToneMapping() {
    const bool enabled = !store.hdrToneMappingEnabled();
    store.setHdrToneMappingEnabled(enabled);
    store.publishChanges();
    emit notificationRequested(
        {enabled ? QCoreApplication::translate("MW", "HDR Tone-Mapping: ON")
                 : QCoreApplication::translate("MW", "HDR Tone-Mapping: OFF"),
         NotificationKind::Info, kToggleNotificationMs});
}

void ViewerToggles::publishModelSwitch() {
    pendingModel.clear();
    store.publishChanges();
}
