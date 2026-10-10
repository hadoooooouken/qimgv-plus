#pragma once

#include <QObject>

#include "gui/ports/viewerport.h"

class ImageViewportController;

// IViewerPort of the Qt Quick UI: forwards to the image viewport controller
// and announces the size of each document it is asked to show (the window
// fits itself to it). The controller must outlive the port. GUI thread only.
class QuickViewerPort final : public QObject, public IViewerPort {
    Q_OBJECT
public:
    explicit QuickViewerPort(ImageViewportController &viewport, QObject *parent = nullptr);

    void showImage(std::shared_ptr<const QImage> image, const QString &filePath) override;
    void showAnimation(const QString &filePath, const QString &format, QSize size) override;
    void closeImage() override;
    void showUpscaledCrop(const QImage &crop, const QRect &originalRect) override;
    void hideUpscaledCrop() override;
    void refreshScaling() override;
    [[nodiscard]] QRect visibleOriginalImageRect() const override;
    [[nodiscard]] float currentScale() const override;
    [[nodiscard]] float devicePixelRatio() const override;
    [[nodiscard]] bool isBusyInteracting() const override;
    [[nodiscard]] bool isRenderingSettled() const override;
    [[nodiscard]] bool panoramaMode() const override;

signals:
    // Emitted before the document is shown; an empty size when an animation
    // does not know its size yet.
    void documentShown(QSize size);

private:
    ImageViewportController &viewport;
};
