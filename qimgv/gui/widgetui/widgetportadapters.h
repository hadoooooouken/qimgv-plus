#pragma once

#include "gui/ports/dialogport.h"
#include "gui/ports/notificationport.h"
#include "gui/ports/shellport.h"
#include "gui/ports/viewerport.h"
#include "gui/ports/windowport.h"

class MW;

// Implementations of the outbound UI ports for the Qt Widgets user
// interface. Each adapter forwards to the main window it was created for;
// the window must outlive the adapter. GUI thread only.

class WidgetNotificationAdapter final : public INotificationPort {
public:
    explicit WidgetNotificationAdapter(MW &window);

    void showNotification(const NotificationRequest &request) override;
    void hideNotifications() override;

private:
    MW &window;
};

class WidgetDialogAdapter final : public IDialogPort {
public:
    explicit WidgetDialogAdapter(MW &window);

    ConfirmationResult confirm(const ConfirmationRequest &request) override;
    FileReplaceDecision resolveFileReplace(const FileReplaceRequest &request) override;
    SavePathResult requestSavePath(const SavePathRequest &request) override;
    std::optional<ResizeRequest> requestResize(QSize initialSize) override;
    TextInputResult requestText(const TextInputRequest &request) override;
    BatchConversionResult runBatchConverter(const BatchConversionRequest &request) override;
    void print(const PrintRequest &request) override;

private:
    MW &window;
};

class WidgetViewerAdapter final : public IViewerPort {
public:
    explicit WidgetViewerAdapter(MW &window);

    [[nodiscard]] DisplayPipeline displayPipeline() const override;
    void showImage(std::shared_ptr<const QImage> image, const QString &filePath) override;
    void showAnimation(const QString &filePath, const QString &format, QSize size) override;
    void closeImage() override;
    void showScaledImage(const QImage &scaled) override;
    void showUpscaledCrop(const QImage &crop, const QRect &originalRect) override;
    void hideUpscaledCrop() override;
    void refreshScaling() override;
    [[nodiscard]] QRect visibleOriginalImageRect() const override;
    [[nodiscard]] float currentScale() const override;
    [[nodiscard]] float devicePixelRatio() const override;
    [[nodiscard]] bool isBusyInteracting() const override;
    [[nodiscard]] bool isRenderingSettled() const override;
    [[nodiscard]] bool panoramaMode() const override;

private:
    MW &window;
};

class WidgetShellAdapter final : public IShellPort {
public:
    explicit WidgetShellAdapter(MW &window);

    void setDirectoryPath(const QString &path) override;
    void setCurrentInfo(const ShellFileInfo &info) override;
    void setMetadata(const MetadataEntries &entries) override;
    void notifySortingChanged(SortingMode mode) override;
    void notifyFolderSortingChanged(SortingMode mode) override;
    void refreshFolderTree(const QString &directoryPath) override;
    void setSaveOverlayVisible(bool visible) override;
    [[nodiscard]] bool isCropPanelActive() const override;
    void toggleCropPanel() override;
    void toggleFullscreenInfoBar() override;
    void toggleRenamePrompt(const QString &currentName) override;

private:
    MW &window;
};

class WidgetWindowAdapter final : public IWindowPort {
public:
    explicit WidgetWindowAdapter(MW &window);

    void showWindow() override;
    void hideWindow() override;
    [[nodiscard]] bool isWindowVisible() const override;
    void setWindowConcealed(bool concealed) override;
    void raiseAndActivateWindow() override;
    [[nodiscard]] WId nativeWindowHandle() const override;
    void saveWindowGeometry() override;
    void setWindowUpdatesSuspended(bool suspended) override;

private:
    MW &window;
};
