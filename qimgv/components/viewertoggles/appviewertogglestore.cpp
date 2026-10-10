#include "appviewertogglestore.h"

#include "settings.h"

AppViewerToggleStore::AppViewerToggleStore(Settings &settings)
    : settings(settings) {}

bool AppViewerToggleStore::useUpscayl() {
    return settings.useUpscayl();
}

void AppViewerToggleStore::setUseUpscayl(bool enabled) {
    settings.setUseUpscayl(enabled);
}

bool AppViewerToggleStore::hdrToneMappingEnabled() {
    return settings.hdrToneMappingEnabled();
}

void AppViewerToggleStore::setHdrToneMappingEnabled(bool enabled) {
    settings.setHdrToneMappingEnabled(enabled);
}

QStringList AppViewerToggleStore::availableUpscaylModels() {
    return settings.availableUpscaylModels();
}

QString AppViewerToggleStore::upscaylModel() {
    return settings.upscaylModel();
}

void AppViewerToggleStore::setUpscaylModel(const QString &model) {
    settings.setUpscaylModel(model);
}

void AppViewerToggleStore::publishChanges() {
    settings.sendChangeNotification();
}
