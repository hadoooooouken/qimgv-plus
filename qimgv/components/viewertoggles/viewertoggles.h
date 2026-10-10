#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "gui/ports/notificationport.h"

// Port of ViewerToggles to the application's settings. The application
// implements it over Settings; the tests use a fake. GUI thread only.
class IViewerToggleStore {
public:
    virtual ~IViewerToggleStore() = default;

    [[nodiscard]] virtual bool useUpscayl() = 0;
    virtual void setUseUpscayl(bool enabled) = 0;
    [[nodiscard]] virtual bool hdrToneMappingEnabled() = 0;
    virtual void setHdrToneMappingEnabled(bool enabled) = 0;
    [[nodiscard]] virtual QStringList availableUpscaylModels() = 0;
    [[nodiscard]] virtual QString upscaylModel() = 0;
    virtual void setUpscaylModel(const QString &model) = 0;
    // Announces the stored values to their listeners (Core re-renders or
    // re-upscales the current image, the UIs refresh their snapshots).
    virtual void publishChanges() = 0;

protected:
    IViewerToggleStore() = default;
    IViewerToggleStore(const IViewerToggleStore &) = default;
    IViewerToggleStore &operator=(const IViewerToggleStore &) = default;
};

// The viewer's setting toggles of both user interfaces: AI upscaling on/off,
// the next Upscayl model and HDR tone mapping on/off, each with its
// confirmation message. Switching the model is announced only once the
// person has stopped switching (kModelSwitchDelayMs), so repeated presses
// step through the models without reloading the upscaler for each, and the
// upscaler's own "AI Upscaling..." message does not replace the model name.
//
// The store must outlive this object. GUI thread only.
class ViewerToggles final : public QObject {
    Q_OBJECT
public:
    static constexpr int kToggleNotificationMs = 600;
    static constexpr int kModelSwitchDelayMs = 1500;

    explicit ViewerToggles(IViewerToggleStore &store, QObject *parent = nullptr);

    void toggleUpscayl();
    // Does nothing when no Upscayl model is installed.
    void cycleUpscaylModel();
    void toggleHdrToneMapping();

signals:
    void notificationRequested(const NotificationRequest &request);
    // AI upscaling was turned off; the viewer drops the upscaled crop.
    void upscaledCropHideRequested();

private:
    void publishModelSwitch();

    IViewerToggleStore &store;
    QTimer modelSwitchTimer;
    // The last model chosen while its announcement is pending; cycling
    // continues from it, not from the model the upscaler has loaded.
    QString pendingModel;
};
