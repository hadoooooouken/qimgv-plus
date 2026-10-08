#pragma once

#include <QImage>
#include <QRect>
#include <QSize>
#include <QString>
#include <memory>

// Outbound UI port for the document viewer: what is displayed and the view
// state the core needs for scaling and AI upscaling decisions. Must only be
// called on the GUI thread.
class IViewerPort {
public:
    virtual ~IViewerPort() = default;

    virtual void showImage(std::shared_ptr<const QImage> image, const QString &filePath) = 0;
    virtual void showAnimation(const QString &filePath, const QString &format, QSize size) = 0;
    virtual void closeImage() = 0;

    // Delivers a CPU-scaled version of the displayed image.
    virtual void showScaledImage(const QImage &scaled) = 0;
    // Displays an AI-upscaled crop over originalRect (source image coordinates).
    virtual void showUpscaledCrop(const QImage &crop, const QRect &originalRect) = 0;
    virtual void hideUpscaledCrop() = 0;
    // Re-requests scaling of the displayed image at the current zoom.
    virtual void refreshScaling() = 0;

    // Visible part of the displayed image, in source image coordinates.
    [[nodiscard]] virtual QRect visibleOriginalImageRect() const = 0;
    [[nodiscard]] virtual float currentScale() const = 0;
    [[nodiscard]] virtual float devicePixelRatio() const = 0;
    // True while the person is zooming or panning.
    [[nodiscard]] virtual bool isBusyInteracting() const = 0;
    // True once the displayed document has been laid out and painted.
    [[nodiscard]] virtual bool isRenderingSettled() const = 0;
    [[nodiscard]] virtual bool panoramaMode() const = 0;

protected:
    IViewerPort() = default;
    IViewerPort(const IViewerPort &) = default;
    IViewerPort &operator=(const IViewerPort &) = default;
};
