#pragma once

#include <QImage>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QSize>
#include <QString>
#include <memory>
#include <rhi/qrhi.h>

class QQuickItem;

// Headless Qt Quick scene on its own Direct3D 11 QRhi: renders the window's
// content into a texture through QQuickRenderControl and reads it back. Works
// with the offscreen QPA platform, unlike window grabs.
class OffscreenQuick {
public:
  OffscreenQuick() = default;
  ~OffscreenQuick();
  OffscreenQuick(const OffscreenQuick &) = delete;
  OffscreenQuick &operator=(const OffscreenQuick &) = delete;

  // Creates the QRhi, the scene and a render target of size pixels. On
  // failure returns false and describes the failing step in error().
  [[nodiscard]] bool create(QSize size);
  [[nodiscard]] const QString &error() const;

  [[nodiscard]] QRhi *rhi() const;
  [[nodiscard]] QQuickItem *contentItem() const;

  // Renders one frame and returns the colour buffer as
  // Format_RGBA8888_Premultiplied, or a null image on failure (see error()).
  [[nodiscard]] QImage render();

private:
  // Records message as error(); returns false for use in create().
  bool fail(const QString &message);

  QSize mSize;
  QString mError;
  // Declared first: the scene graph releases its QRhi resources while the
  // window and the render control are destroyed.
  std::unique_ptr<QRhi> mRhi;
  std::unique_ptr<QRhiTexture> mColorBuffer;
  std::unique_ptr<QRhiRenderBuffer> mDepthStencil;
  std::unique_ptr<QRhiTextureRenderTarget> mRenderTarget;
  std::unique_ptr<QRhiRenderPassDescriptor> mRenderPass;
  std::unique_ptr<QQuickRenderControl> mControl;
  std::unique_ptr<QQuickWindow> mWindow;
};
