#pragma once

#include "components/viewertoggles/viewertoggles.h"

class Settings;

// IViewerToggleStore over the application's Settings; publishChanges() is
// Settings::sendChangeNotification(). The settings must outlive the store.
// GUI thread only.
class AppViewerToggleStore final : public IViewerToggleStore {
public:
    explicit AppViewerToggleStore(Settings &settings);

    [[nodiscard]] bool useUpscayl() override;
    void setUseUpscayl(bool enabled) override;
    [[nodiscard]] bool hdrToneMappingEnabled() override;
    void setHdrToneMappingEnabled(bool enabled) override;
    [[nodiscard]] QStringList availableUpscaylModels() override;
    [[nodiscard]] QString upscaylModel() override;
    void setUpscaylModel(const QString &model) override;
    void publishChanges() override;

private:
    Settings &settings;
};
