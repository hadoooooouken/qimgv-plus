#pragma once

#include "gui/ports/viewerport.h"

class ImageViewportController;

// IViewerPort of the Qt Quick UI: forwards to the image viewport controller.
// The controller must outlive the port. GUI thread only.
class QuickViewerPort final : public IViewerPort {
public:
    explicit QuickViewerPort(ImageViewportController &viewport);

    void showImage(std::shared_ptr<const QImage> image, const QString &filePath) override;
    void showAnimation(const QString &filePath, const QString &format, QSize size) override;
    void closeImage() override;
    // The GPU renderer filters and scales the image itself; the CPU-scaled
    // copy that Core delivers (only requested as the AI upscaler's source)
    // is not displayed.
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
    ImageViewportController &viewport;
};
